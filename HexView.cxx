#include <string.h>

#include <map>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Menu_Button.H>

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

	addrWidth32 = lineHeight = 0;
	fl_measure("DEADBEEF ", addrWidth32, lineHeight); 

	addrWidth64 = lineHeight = 0;
	fl_measure("DEADBEEFCAFEBABE ", addrWidth64, lineHeight);

	charWidth = lineHeight = 0;
	fl_measure("_", charWidth, lineHeight); 

	byteWidth = lineHeight = 0;
	fl_measure("00 ", byteWidth, lineHeight); 
	printf("(bytesWidth,lineHeight)=(%d,%d)\n", byteWidth, lineHeight);

	bytesWidth = lineHeight = 0;
	fl_measure("00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF  ", bytesWidth,
		lineHeight);

	asciiWidth = lineHeight = 0;
	fl_measure("abcdefghijklmnop", asciiWidth, lineHeight);
   
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
	marginTop = 2;
	marginBottom = 2;

	int width = marginLeft + addrWidth + bytesWidth + asciiWidth + marginRight;
	int height = lineHeight * 32;
	printf("HexView: I prefer to be %d x %d\n", width, height);
	//resize(x(), y(), width, height);

	/* initial color, selection */
	//selActive = 0;
	cursorOffs = 0;
}

/*****************************************************************************/
/* main API */
/*****************************************************************************/
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
	//printf("setBytes(addr=%016llX, data=<ptr>, len=0x%X)\n", addr, len);
	//dump_bytes(data, len, (uintptr_t)0);

	/* maybe clear highlight data? */

	/* clear the selection */
	selEditing = selActive = 0;
	addrSelStart = addrSelEnd = 0;

	addrStart = addr;
	addrEnd = addr + len; // non-inclusive ')' endpoint
	nBytes = len;

	bytes = (uint8_t *)malloc(len);
	memcpy(bytes, data, len);

	setView(addr);
	
	if(callback) callback(HV_CB_NEW_BYTES, 0);
}

void HexView::clearBytes(void)
{
	hlClear();

	addrStart = addrEnd = 0;
	addrViewStart = addrViewEnd = 0;

	if(bytes) free(bytes);
	bytes = NULL;
	nBytes = 0;
	
	setView(0);
}

/* set the view, redraw, update variables */
void HexView::setView(uint64_t addr)
{
	// filter address
	if(addr > addrViewEnd) addr = addrViewEnd;
	if(addr % 16) addr -= (addr % 16);

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

void HexView::setSelection(uint64_t start, uint64_t end)
{
	if(start < addrEnd || end < addrEnd || end >= start) {
		selEditing=0;
		selActive=1;
		addrSelStart = start;
		addrSelEnd = end;
		redraw();
		if(callback) callback(HV_CB_SELECTION, 0);
	}
	else {
		printf("ERROR: [%016llX,%016llX) is invalid\n", start, end);
	}
}

void HexView::setShowAddress(bool b)
{
	showAddress = b;
	redraw();
}

void HexView::setShowAscii(bool b)
{
	showAscii = b;
	redraw();
}

/*****************************************************************************/
/* drawing helpers */
/*****************************************************************************/
int HexView::viewAddrToBytesXY(uint64_t addr, int *ax, int *ay)
{
	if(addr < addrViewStart || addr >= addrViewEnd) return -1;

	int offs = addr - addrViewStart;
	int lineNum = offs / 16;
	int colNum = offs % 16;

	*ax = x() + marginLeft + colNum*byteWidth;
	*ay = y() + marginTop + lineNum*lineHeight;

	if(showAddress) *ax += addrWidth;

	return 0;
}

int HexView::viewAddrToAsciiXY(uint64_t addr, int *ax, int *ay)
{
	if(addr < addrViewStart || addr >= addrViewEnd) return -1;

	int offs = addr - addrViewStart;
	int lineNum = offs / 16;
	int colNum = offs % 16;

	*ax = x() + marginLeft + bytesWidth + colNum*charWidth;
	*ay = y() + marginTop + lineNum*lineHeight;

	if(showAddress) *ax += addrWidth;

	return 0;
}

/*****************************************************************************/
/* highlighting API */
/*****************************************************************************/
void HexView::hlAdd(uint64_t left, uint64_t right, uint32_t color)
{
	hlRanges.add(Interval(left, right, color));
	redraw();
}

void HexView::hlAdd(uint64_t left, uint64_t right)
{
	hlRanges.add(Interval(left, right, autoPalette[autoPaletteIdx]));
	autoPaletteIdx = (autoPaletteIdx+1) % 16;
	redraw();
}

void HexView::hlClear(void)
{
	hlRanges.clear();
	autoPaletteIdx = 0;
	redraw();
}

/*****************************************************************************/
/* draw */
/*****************************************************************************/
void HexView::draw(void)
{
	char buf[256];
	int i,x1,y1,x2,y2;
   
	fl_font(FL_COURIER, FL_NORMAL_SIZE);
	int r2c_bias_y = fl_height() - fl_descent();

	/* note that the point you specify to draw at (x,y) is 0,0 on screen's top
		left corner, but is on text's bottom left corner (which is why we have
		(line+1) */
	fl_draw_box(FL_BORDER_BOX, x(), y(), w(), h(), fl_rgb_color(255, 255, 255));
	/* draw the addresses */
	if(showAddress) { 
		fl_color(0x00640000);
		for(i=0; i<linesPerPage; ++i) {
			if(addrMode==32) {
				sprintf(buf, "%08llX ", addrViewStart+i*16);
			}
			if(addrMode==64) {
				sprintf(buf, "%16llX ", addrViewStart+i*16);
			}

			fl_draw(buf, x()+marginLeft, y()+marginTop+i*lineHeight+r2c_bias_y);
		}
	}

	/* draw the bytes */
	#define TO_RGB_COLOR(p) fl_rgb_color(((p)&0xFF0000)>>16, ((p)&0xFF00)>>8, (p&0xFF))
	#define SET_PACKED_COLOR(p) fl_color((p)<<8)
	uint8_t *b = bytes + (addrViewStart - addrStart);
	for(uint64_t addr=addrViewStart; addr<addrViewEnd; ++addr, ++b) {
		uint32_t color = 0xFFFFFF;
		Interval ival(0,0);

		/* 
			draw the byte
		*/
		viewAddrToBytesXY(addr, &x1, &y1);
		viewAddrToAsciiXY(addr, &x2, &y2);

		/* highlighter? */
		if(hlRanges.search(addr, ival)) {
			// sort by size
			color = ival.data_u32; 
			//printf("search hit for addr 0x%llx, color is: %X\n", addr, color);
			SET_PACKED_COLOR(color);
			fl_rectf(x1-charWidth/2, y1-1, 3*charWidth, lineHeight);
			if(showAscii) {
				fl_rectf(x2, y2, charWidth, lineHeight);
			}
		}

		/* selection? */
		if(selActive && addr>=addrSelStart && addr<addrSelEnd) {
			color = 0xFF00ff;
			SET_PACKED_COLOR(color);
			fl_rectf(x1, y1-1, 3*charWidth, lineHeight);
			if(showAscii) {
				fl_rectf(x2, y2, charWidth, lineHeight);
			}
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
		fl_draw(buf, x1, y1+r2c_bias_y);

		if(showAscii) {
			sprintf(buf, "%c", ((b[0])>=' ' && (b[0])<='~') ? (b[0]) : '.');
			fl_draw(buf, x2, y2+r2c_bias_y);
		}
	}

	/* draw the cursor */
	if(Fl::focus() == this) { 
		fl_color(0xff000000);
		viewAddrToBytesXY(addrViewStart + cursorOffs, &x1, &y1);
		viewAddrToAsciiXY(addrViewStart + cursorOffs, &x2, &y2);
		fl_rect(x1, y1, charWidth*2, lineHeight);
		fl_rect(x2, y2, charWidth, lineHeight);
	}
}

int HexView::handle(int event)
{
	int x, y;
	int rc = 0; /* 0 if not used or understood, 1 if event was used and can be deleted */

	if(event == FL_FOCUS) {
		//printf("FL_FOCUS %d\n", cnt);
		rc = 1;
	}
	else if(event == FL_UNFOCUS) {
		/* To receive FL_KEYBOARD events you must also respond to the FL_FOCUS
			and FL_UNFOCUS events by returning 1. */
		//printf("FL_UNFOCUS %d\n", cnt);
		redraw();
		rc = 1;
	}
	else if(event == FL_ENTER) {
		//printf("FL_ENTER %d!\n", cnt);
	}
	else if(event == FL_LEAVE) {
		//printf("FL_LEAVE %d!\n", cnt);
	}
	else if(event == FL_KEYDOWN) {
		//printf("FL_KEYDOWN %d!\n", cnt);

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
		//printf("FL_KEYUP\n");
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
	else
	if(event == FL_PUSH) {
		//printf("FL_PUSH!\n");
		Fl::focus(this);
		redraw();
		rc = 1;
	}
	else
	if(event == FL_RELEASE) {
		//printf("FL_RELEASE\n");
	}
	else {
		//printf("unrecognized event: %d\n", event);
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

