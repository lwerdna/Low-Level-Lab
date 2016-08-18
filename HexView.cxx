#include <string.h>

#include <map>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>

#include <FL/Fl_Window.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>

#include <FL/fl_draw.H>

extern "C" {
#include <autils/output.h>
}

#include "HexView.h"

HexView::HexView(int x_, int y_, int w, int h, const char *label): 
    Fl_Widget(x_, y_, w, h, label)
{
    fl_font(FL_COURIER, FL_NORMAL_SIZE);

    bytesPerLine = 16;

    fl_measure("DEADBEEF: ", addrWidth32, lineHeight); 
    fl_measure("DEADBEEFCAFEBABE: ", addrWidth64, lineHeight); 
    fl_measure("0", charWidth, lineHeight); 
    printf("lineHeight: %d\n", lineHeight);
    fl_measure("00 ", byteWidth, lineHeight); 
    printf("lineHeight: %d\n", lineHeight);
    fl_measure("00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF  ", bytesWidth,
        lineHeight);

    fl_measure("abcdefghijklmnop", asciiWidth, lineHeight);
    printf("lineHeight: %d\n", lineHeight);
   
    /* set mode, calculate address width */
    addrMode = 32;
    if(addrMode == 32) {
        addrWidth = addrWidth32;
    }
    else if(addrMode == 64) {
        addrWidth = addrWidth64;
    }

    /* margins */
    marginLeft = marginRight = 2;
    marginTop = 0;
    marginBottom = 2;

    int width = marginLeft + addrWidth + bytesWidth + asciiWidth + marginRight;
    int height = lineHeight * 32;
    printf("HexView: I prefer to be %d x %d\n", width, height);
    //resize(x(), y(), width, height);

    /* initial color, selection */
    //selActive = 0;
    cursorOffs = 0;
}

void HexView::setBytes(uint64_t addr, unsigned char *data, int len)
{
    /* maybe clear highlight data? */

    printf("setBytes(addr=%016llX, data=<ptr>, len=0x%X\n", addr, len);
    dump_bytes(data, len, (uintptr_t)0);

    addrStart = addr;
    addrEnd = addr + len; // non-inclusive ')' endpoint
    bytes = data;
    redraw();
}

void HexView::clearBytes(void)
{
    addrStart = addrEnd = 0;
    bytes = NULL;
    redraw();
}

/*****************************************************************************/
/* drawing helpers */
/*****************************************************************************/
int HexView::viewAddrToBytesXY(uint64_t addr, int *x, int *y)
{
    uint64_t left, right;
    getAddrRangeInView(&left, &right);
    if(addr < left || addr >= right) return -1;

    int offs = addr - addrView;

    int lineNum = offs / 16;
    int colNum = offs % 16;

    *x = Fl::x() + marginLeft + addrWidth + colNum*byteWidth;
    *y = Fl::y() + marginTop + lineNum*lineHeight;

    return 0;
}

int HexView::viewAddrToAsciiXY(uint64_t addr, int *x, int *y)
{
    if(0 != viewAddrToBytesXY(addr, x, y)) return -1;
    *x += bytesWidth;
    return 0;
}

void HexView::draw(void)
{
    char buf[256];
   
    if(!bytes) {
        return;
    }

    fl_font(FL_COURIER, FL_NORMAL_SIZE);

    int x0 = x() + marginLeft;
    int y0 = y() + marginTop;
    int w0 = w();
    int h0 = h();

    //dbgprintf("x0,y0,w0,h0 = (%d,%d,%d,%d)\n", x0, y0, w0, h0);

    /* note that the point you specify to draw at (x,y) is 0,0 on screen's top
        left corner, but is on text's bottom left corner (which is why we have
        (line+1) */

    /* draw the following to enable debugging of margins/border */

    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), fl_rgb_color(255, 255, 255));
    /*
    fl_draw_box(FL_FLAT_BOX, 
                x() + marginLeft, 
                y() + marginTop, 
                w() - marginLeft - marginRight, 
                h() - marginTop - marginBottom, 
                FL_GREEN);
    */

    int nBytesToDraw = getNumBytesInView();
    int nLinesToDraw = (nBytesToDraw + (bytesPerLine-1)) / bytesPerLine;

    /* draw the addresses */
    uint64_t addrCurr = addrView;

    fl_color(0x00640000);

    for(int i=0; i<nLinesToDraw; ++i) {
        if(addrMode == 32) {
            sprintf(buf, "%08llX: ", addrCurr);
        }
        else if(addrMode == 64) {
            sprintf(buf, "%016llX: ", addrCurr);
        }
       
        fl_draw(buf, x0+0, y0 + (i+1)*lineHeight);

        addrCurr += bytesPerLine;
    }

    /* draw the highlights */
//        if(selActive && addr_ >= selAddrStart && addr_ <= selAddrEnd) {
//            fl_color(0xffff0000);
//            fl_rectf(x0+addrWidth + bi*byteWidth, y0+(line*lineHeight)+4, charWidth*2, lineHeight);
//        }

    /* draw the selection (trumps the highlights) */
    if(selActive) {
        fl_color(0xFFFF0000);

        int nBytesLeft = nBytesToDraw;
    
        for(int curLine=0; nBytesLeft>0; ++curLine) {
            int addrLineStart = addrView + curLine*bytesPerLine;
            int addrLineEnd = addrLineStart + std::min(bytesPerLine, nBytesLeft);

            #define WITHIN(X) ((X)>=selAddrStart && (X)<=selAddrEnd)
            int cond;
            if(WITHIN(addrLineStart)) cond += 2;
            if(WITHIN(addrLineEnd)) cond += 1;

            uint64_t addrHlStart = -1;
            uint64_t addrHlEnd = -1;

            printf("selection conditon: %d\n", cond);
            switch(cond) {
                case 0: // neither are within the selection, do nothing
                    break;
                case 1: // the RHS is within the selection, but not the left
                    addrHlStart = selAddrStart;
                    addrHlEnd = addrLineEnd;
                    break;
                case 2: // the LHS is within the selection, but not the right
                    addrHlStart = addrLineEnd;
                    addrHlEnd = selAddrEnd;
                    break;
                case 3: // they're both in, highlight the whole line
                    addrHlStart = addrLineStart;
                    addrHlEnd = addrLineEnd;
                    break;
            }

            if(addrHlStart != -1) {
                int offsHlStart = addrHlStart - addrLineStart;
                int offsHlEnd = addrHlEnd - addrLineStart;
                int numHlChars = offsHlEnd - offsHlStart;

                int rectX = x0+addrWidth + offsHlStart*byteWidth;
                int rectY = y0+(curLine*lineHeight);
                fl_rectf(rectX-1, rectY+3, (3*charWidth-1)*numHlChars+3, lineHeight+1);
            }
                                     
            if(nBytesLeft < 16) {
                nBytesLeft = 0;
            }
            else {
                nBytesLeft -= 16;
            }
    
        }
    }

    /* draw the bytes */
    uint8_t *b = bytes + (addrView - addrStart);

    int nBytesLeft = nBytesToDraw;

    for(int curLine=0; nBytesLeft>0; ++curLine) {
        switch(nBytesLeft) {
            case 0: break;
            case 1: sprintf(buf, "%02X", b[0]); break;
            case 2: sprintf(buf, "%02X %02X", b[0],b[1]); break;
            case 3: sprintf(buf, "%02X %02X %02X", b[0],b[1],b[2]); break;
            case 4: sprintf(buf, "%02X %02X %02X %02X", b[0],b[1],b[2],b[3]); break;
            case 5: sprintf(buf, "%02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4]); break;
            case 6: sprintf(buf, "%02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5]); break;
            case 7: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6]); break;
            case 8: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7]); break;
            case 9: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8]); break;
            case 10: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9]); break;
            case 11: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10]); break;
            case 12: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11]); break;
            case 13: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12]); break;
            case 14: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13]); break;
            case 15: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14]); break;
            default: sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]); break;
        }

        if(nBytesLeft < 16) {
            nBytesLeft = 0;
        }
        else {
            nBytesLeft -= 16;
            b += 16;
        }

        /* if the highlight is light, draw dark, and visa versa */
        fl_color(0x000000);

        printf("drawing text at (%d, %d)\n", x0+addrWidth, y0+(curLine+1)*lineHeight);
        fl_draw(buf, x0+addrWidth, y0+(curLine+1)*lineHeight);
    }

    /* draw the ascii */  
    #define B2A(X) ( ((X)>=' ' && (X)<='~') ? (X) : '.' )
    nBytesLeft = nBytesToDraw;
    b = bytes + (addrView - addrStart);

    for(int curLine=0; nBytesLeft>0; ++curLine)
    {
        switch(nBytesLeft) {
            case 0: break;
            case 1: sprintf(buf, "%c", B2A(b[0])); break;
            case 2: sprintf(buf, "%c%c", B2A(b[0]),B2A(b[1])); break;
            case 3: sprintf(buf, "%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2])); break;
            case 4: sprintf(buf, "%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3])); break;
            case 5: sprintf(buf, "%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4])); break;
            case 6: sprintf(buf, "%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5])); break;
            case 7: sprintf(buf, "%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6])); break;
            case 8: sprintf(buf, "%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7])); break;
            case 9: sprintf(buf, "%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8])); break;
            case 10: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9])); break;
            case 11: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9]),B2A(b[10])); break;
            case 12: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9]),B2A(b[10]),B2A(b[11])); break;
            case 13: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9]),B2A(b[10]),B2A(b[11]),B2A(b[12])); break;
            case 14: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9]),B2A(b[10]),B2A(b[11]),B2A(b[12]),B2A(b[13])); break;
            case 15: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9]),B2A(b[10]),B2A(b[11]),B2A(b[12]),B2A(b[13]),B2A(b[14])); break;
            default: sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", B2A(b[0]),B2A(b[1]),B2A(b[2]),B2A(b[3]),B2A(b[4]),B2A(b[5]),B2A(b[6]),B2A(b[7]),B2A(b[8]),B2A(b[9]),B2A(b[10]),B2A(b[11]),B2A(b[12]),B2A(b[13]),B2A(b[14]),B2A(b[15])); break;
        }

        if(nBytesLeft <= 16) {
            nBytesLeft = 0;
        }
        else {
            nBytesLeft -= 16;
            b += 16;
        }

        /* if the highlight is light, draw dark, and visa versa */
        fl_color(0x0000FF00);
        fl_draw(buf, x0+addrWidth+bytesWidth, y0+((curLine+1) * lineHeight));
    }

    /* draw the cursor */
    fl_color(0xff000000);
    int cursorRow = cursorOffs / 16;
    int cursorCol = cursorOffs % 16;
    //printf("cursorRow: %d\n", cursorRow);
    //printf("cursorCol: %d\n", cursorCol);

    int rectX = x0+ addrWidth + cursorCol*byteWidth;
    int rectY = y0+ cursorRow*lineHeight;
    fl_rect(rectX-1, rectY+3, charWidth*2+3, lineHeight+1);

    rectX = x0+addrWidth+bytesWidth + cursorCol*charWidth;
    rectY = y0+(cursorRow*lineHeight);
    fl_rect(rectX-1, rectY+3, charWidth+3, lineHeight+1);
}

int HexView::handle(int event)
{
    int x, y;
    int rc = 0; /* 0 if not used or understood, 1 if event was used and can be deleted */


    if(event == FL_FOCUS || event == FL_UNFOCUS) {
        /* To receive FL_KEYBOARD events you must also respond to the FL_FOCUS
            and FL_UNFOCUS events by returning 1. */
        rc = 1;
    }
    else
    if(event == FL_KEYDOWN) {
        int numBytesInView = getNumBytesInView();

        int keyCode = Fl::event_key();

        switch(keyCode) {
            case FL_Shift_L:
            case FL_Shift_R:
                selAddrStart = selAddrEnd = addrView + cursorOffs;
                printf("selection started to [%016llx,%016llx]\n", selAddrStart, selAddrEnd);
                selEditing = selActive = 1;
                rc = 1;
                break;

            case FL_Up:
            case FL_Left:
            case FL_Right:
            case FL_Down:
            {
                int delta;
                if(keyCode == FL_Up) delta = -16;
                else if(keyCode == FL_Left) delta = -1;
                else if(keyCode == FL_Right) delta = 1;
                else if(keyCode == FL_Down) delta = 16;

                int hypoth = cursorOffs + delta;
                if(hypoth < 0 || hypoth >= numBytesInView) {
                    printf("can't\n");
                }
                else {
                    cursorOffs += delta;
                    if(selEditing) {
                        selAddrEnd += delta;
                        printf("selection modified to [%016llx,%016llx]\n", selAddrStart, selAddrEnd);
                    }
                    rc = 1;
                    redraw();
                }
                break;
            }

            case FL_Escape:
                selActive = 0;
                rc = 1;
                redraw();
                break;
        }
    }
    else
    if(event == FL_KEYUP) {
        int keyCode = Fl::event_key();

        switch(keyCode) {
            case FL_Shift_L:
            case FL_Shift_R:
                selEditing = 0;
                printf("selection closed to [%016llx,%016llx]\n", selAddrStart, selAddrEnd);
                rc = 1;
                break;
        }
    }

    return rc;
}

void HexView::resize(int x, int y, int w, int h) {
    //dbgprintf("resize (%d,%d,%d,%d)\n", x, y, w, h);
    this->x(x);
    this->y(y);
    this->h(h);
    this->w(w);
    return;
}

int HexView::getNumLinesCapacity()
{
    return (h() - marginTop - marginBottom) / lineHeight;
}

int HexView::getNumBytesCapacity()
{
    return getNumLinesCapacity() * bytesPerLine;
}

int HexView::getNumBytesInView()
{
    int capacity = getNumBytesCapacity();
    int bytesFromView = addrEnd - addrView;
    return std::min(capacity, bytesFromView);
}

int HexView::getNumLinesInView()
{
    int n = getNumBytesInView();
    return (n + (bytesPerLine-1))/bytesPerLine;
}

void HexView::getAddrRangeInView(uint64_t *start, uint64_t *end)
{
    *start = addrView;
    *end = addrView + getNumBytesInView();
}
