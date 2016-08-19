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

void HexView::setCallback(HexView_callback cb)
{
    callback = cb;
}

void HexView::clrCallback(void)
{
    callback = NULL;
}

void HexView::setBytes(uint64_t addr, unsigned char *data, int len)
{

    printf("setBytes(addr=%016llX, data=<ptr>, len=0x%X\n", addr, len);
    //dump_bytes(data, len, (uintptr_t)0);

    /* maybe clear highlight data? */

    /* clear the selection */
    selEditing = selActive = 0;
    addrSelStart = addrSelEnd = 0;

    addrStart = addr;
    addrEnd = addr + len; // non-inclusive ')' endpoint
    nBytes = addrEnd - addrStart;
    bytes = data;
    setView(addr);
    
    if(callback) callback(HV_CB_NEW_BYTES, 0);
}

void HexView::clearBytes(void)
{
    addrStart = addrEnd = 0;
    bytes = NULL;
    setView(0);
}

/* set the view, redraw, update variables */
void HexView::setView(uint64_t addr)
{
    // capacity info
    linesPerPage = (h() - marginTop - marginBottom) / lineHeight;
    bytesPerPage = linesPerPage * bytesPerLine;

    // range of displayed addresses
    addrViewStart = addr;
    if(addrViewStart + bytesPerPage >= addrEnd)
        addrViewEnd = addrEnd;
    else
        addrViewEnd = addr + bytesPerPage;

    // amount of bytes, lines 
    bytesInView = std::min(bytesPerPage, (int)(addrViewEnd - addrViewStart));
    linesInView = (bytesInView + (bytesPerLine-1)) / bytesPerLine;
   
    //
    pageTotal = (nBytes + (bytesPerPage-1))/bytesPerPage;
    uint64_t viewOffs = addrViewStart - addrStart;
    pageCurrent = (viewOffs + (bytesPerPage-1))/bytesPerPage;
    viewPercent = 100.0 * (((float)pageCurrent+1.0)/(float)pageTotal);
    
    if(callback) callback(HV_CB_VIEW_MOVE, 0);

    redraw();
}

void HexView::setView()
{
    setView(addrViewStart);
}

/*****************************************************************************/
/* drawing helpers */
/*****************************************************************************/
int HexView::viewAddrToAddressesXY(uint64_t addr, int *x, int *y)
{
    if(addr < addrViewStart || addr >= addrViewEnd) return -1;

    int offs = addr - addrViewStart;
    int lineNum = offs / 16;

    *x = Fl::x() + marginLeft;
    *y = Fl::y() + marginTop + lineNum*lineHeight;

    return 0;
}

int HexView::viewAddrToBytesXY(uint64_t addr, int *x, int *y)
{
    if(addr < addrViewStart || addr >= addrViewEnd) return -1;

    int offs = addr - addrViewStart;
    int lineNum = offs / 16;
    int colNum = offs % 16;

    *x = Fl::x() + marginLeft + addrWidth + colNum*byteWidth;
    *y = Fl::y() + marginTop + lineNum*lineHeight;

    return 0;
}

int HexView::viewAddrToAsciiXY(uint64_t addr, int *x, int *y)
{
    if(addr < addrViewStart || addr >= addrViewEnd) return -1;

    int offs = addr - addrViewStart;
    int lineNum = offs / 16;
    int colNum = offs % 16;

    *x = Fl::x() + marginLeft + addrWidth + bytesWidth + colNum*charWidth;
    *y = Fl::y() + marginTop + lineNum*lineHeight;

    return 0;
}

void HexView::hlAdd(uint64_t left, uint64_t right, uint32_t color)
{
    printf("adding color to list: %08X\n", color);
    hlRanges.add(left, right, color);
}

void HexView::draw(void)
{
    char buf[256];
    int x1,y1,x2,y2;

    if(!bytes) {
        return;
    }

    fl_font(FL_COURIER, FL_NORMAL_SIZE);

    //dbgprintf("x0,y0,w0,h0 = (%d,%d,%d,%d)\n", x0, y0, w0, h0);

    /* note that the point you specify to draw at (x,y) is 0,0 on screen's top
        left corner, but is on text's bottom left corner (which is why we have
        (line+1) */

    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), fl_rgb_color(255, 255, 255));

    /* draw the addresses */
    fl_color(0x00640000);

    for(uint64_t addr=addrViewStart; addr<addrViewEnd; addr+=16) {
        viewAddrToAddressesXY(addr, &x1, &y1);
        if(addrMode == 32) {
            sprintf(buf, "%08llX: ", addr);
        }
        else if(addrMode == 64) {
            sprintf(buf, "%016llX: ", addr);
        }
       
        fl_draw(buf, x1, y1+lineHeight);
    }

    /* draw the bytes */
    //#define SET_PACKED_COLOR(p) fl_rgb_color(((p)&0xFF0000)>>16, ((p)&0xFF00)>>8, (p&0xFF))
    #define SET_PACKED_COLOR(p) fl_color((p)<<8)
    uint8_t *b = bytes + (addrViewStart - addrStart);
    for(uint64_t addr=addrViewStart; addr<addrViewEnd; ++addr, ++b) {
        uint32_t color = 0xFFFFFF;
        int x1,y1,x2,y2;

        /* 
            draw the byte
        */
        viewAddrToBytesXY(addr, &x1, &y1);
        viewAddrToAsciiXY(addr, &x2, &y2);

        /* highlighter? */
        if(hlRanges.searchFast(addr, &color)) {
            SET_PACKED_COLOR(color);
            fl_rectf(x1-1, y1+3, 3*charWidth, lineHeight);
            fl_rectf(x2, y2+3, charWidth, lineHeight);
        }

        /* selection? */
        if(selActive && addr>=addrSelStart && addr<addrSelEnd) {
            color = 0xFFFF00;
            SET_PACKED_COLOR(color);
            fl_rectf(x1-1, y1+3, 3*charWidth, lineHeight);
            fl_rectf(x2, y2+3, charWidth, lineHeight);
        }

        float luma = 1 - (.299*(((color)&0xFF0000)>>16) + .587*(((color)&0xFF00)>>8) + .114*(color&0xFF))/255.0; // thx stackoverflow
        if(luma < .5) {
            color = 0;
            SET_PACKED_COLOR(color);
        }
        else {
            color = 0xFFFFFF;
            SET_PACKED_COLOR(color);
        }
        sprintf(buf, "%02X", b[0]);
        fl_draw(buf, x1, y1+lineHeight);
        sprintf(buf, "%c", ((b[0])>=' ' && (b[0])<='~') ? (b[0]) : '.');
        fl_draw(buf, x2, y2+lineHeight);
    }

    /* draw the cursor */
    fl_color(0xff000000);
    viewAddrToBytesXY(addrViewStart + cursorOffs, &x1, &y1);
    viewAddrToAsciiXY(addrViewStart + cursorOffs, &x2, &y2);
    fl_rect(x1-1, y1+3, charWidth*2+3, lineHeight);
    fl_rect(x2-1, y2+3, charWidth+3, lineHeight);
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
        uint64_t sampleAddrView = addrViewStart;
        int sampleCursorOffs = cursorOffs;

        int keyCode = Fl::event_key();

        switch(keyCode) {
            case FL_Shift_L:
            case FL_Shift_R:
                addrSelStart = addrViewStart + cursorOffs;
                addrSelEnd = addrSelStart + 1; // remember RHS is ')' inclusion
                if(callback) callback(HV_CB_SELECTION, 0);
                //printf("selection started to [%016llx,%016llx)\n", addrSelStart, addrSelEnd);
                selEditing = selActive = 1;
                rc = 1;
                redraw();
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
    
                int hypothOffs = cursorOffs + delta;
                uint64_t hypothAddr = addrViewStart + cursorOffs + delta;
                //printf("---------\n");
                //printf("hypothOffs: %d\n", hypothOffs);
                //printf("hypothAddr: %016llx\n", hypothAddr);
                //printf("addrViewStart: %016llx\n", addrViewStart);
                //printf("addrViewEnd: %016llx\n", addrViewEnd);

                /* scroll up? */
                if(hypothAddr < addrViewStart && hypothAddr >= addrStart) {
                    uint64_t addrNew = addrViewStart - bytesPerLine;
                    cursorOffs = hypothAddr - addrNew;
                    setView(addrNew);
                }
                /* scroll down? */
                else
                if(hypothAddr >= addrViewEnd &&
                  hypothAddr < addrEnd) {
                    uint64_t addrNew = addrViewStart + bytesPerLine;
                    cursorOffs = hypothAddr - addrNew;
                    setView(addrNew);
                }
                /* cannot decrease */
                else
                if(hypothAddr < addrStart) {
                    printf("can't move left\n");
                }
                else
                if(hypothAddr >= addrEnd) {
                    printf("can't move right\n");
                }
                else {
                    cursorOffs += delta;
                    redraw();
                }
                
                rc = 1;
                break;
            }

            case FL_Home:
                setView(addrStart);
                break;

            case FL_End:
                if(addrStart + bytesPerPage >= addrEnd) {
                    setView(addrStart);
                }
                else {
                    printf("addrEnd: %llX\n", addrEnd);
                    printf("bytesPerPage: %d\n", bytesPerPage);
                    uint64_t addrNew = addrEnd - bytesPerPage;
                    addrNew += bytesPerLine - (addrNew % bytesPerLine);
                    printf("addrNew: %llX\n", addrNew);
                    setView(addrNew);
                }
                break;

            case FL_Page_Up:
                if(addrViewStart == addrStart) {
                    printf("at page 0\n");
                }
                else {
                    if(addrStart + bytesPerPage < addrViewStart) {
                        addrViewStart -= bytesPerPage;
                    }
                    else {
                        addrViewStart = addrStart;
                    }
                    setView();
                }
                break;
            
            case FL_Page_Down:
                if(addrViewStart + bytesPerPage >= addrEnd) {
                    printf("at last page\n");
                }
                else {
                    uint64_t addrLastPage = addrEnd - 1 - bytesPerPage;
                    addrLastPage -= addrLastPage % bytesPerLine;

                    if(addrViewStart + bytesPerPage > addrLastPage) {
                        addrViewStart = addrLastPage;
                    }
                    else {
                        addrViewStart += bytesPerPage;
                    }
                    setView();
                }
                break;

            case FL_Escape:
                selActive = 0;
                if(callback) callback(HV_CB_SELECTION, 0);
                //printf("selection cleared\n");
                rc = 1;
                redraw();
                break;
        }

        /* if the view or cursor has changed... */
        if(sampleAddrView != addrViewStart || sampleCursorOffs != cursorOffs) {
            /* modify the selection? */
            if(selEditing) {
                addrSelEnd = addrViewStart + cursorOffs + 1;
                if(callback) callback(HV_CB_SELECTION, 0);
                //printf("selection modified to [%016llx,%016llx]\n", addrSelStart, addrSelEnd);
            }
        }
    }
    else
    if(event == FL_KEYUP) {
        int keyCode = Fl::event_key();

        switch(keyCode) {
            case FL_Shift_L:
            case FL_Shift_R:
                selEditing = 0;
                if(callback) callback(HV_CB_SELECTION, 0);
                //printf("selection closed to [%016llx,%016llx]\n", addrSelStart, addrSelEnd);
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
    setView(addrViewStart);
    return;
}

