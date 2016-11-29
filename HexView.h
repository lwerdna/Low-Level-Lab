#pragma once

#include <map>
#include <string>
#include <vector>
using namespace std;

#include <FL/Fl_Widget.H>

#include "IntervalMgr.h"

typedef void (*HexView_callback)(int type, void *data);

#define HV_CB_SELECTION 0 /* hexview reports a selection change */
#define HV_CB_VIEW_MOVE 1 /* hexview reports the view has moved */
#define HV_CB_NEW_BYTES 2 /* hexview reports its loaded new bytes */

class HexView : public Fl_Widget {
    public:
    HexView(int X, int Y, int W, int H, const char *label=0);
 
    /* the entire memory buffer */
    int addrMode; // 32 or 64
    uint64_t addrStart, addrEnd;
    uint8_t *bytes;
    int nBytes;
   
    void setCallback(HexView_callback cb);
    void clrCallback(void);

    void setBytes(uint64_t addr, uint8_t *bytes, int len);
    void setView(uint64_t addr);
    void setView();
    void setSelection(uint64_t start, uint64_t end);
    void clearBytes(void);
	void setShowAddress(bool);
	void setShowAscii(bool);
    int handle(int event);
    void draw();
    void resize(int, int, int, int);
    
    /* live view information */
    int linesPerPage;
    int bytesPerPage;
    int linesInView;
    int bytesInView;
    int pageTotal, pageCurrent;
    float viewPercent;
    uint64_t addrViewStart, addrViewEnd; // [,)

    int viewAddrToBytesXY(uint64_t addr, int *x, int *y);
    int viewAddrToAsciiXY(uint64_t addr, int *x, int *y);
    int viewAddrToAddressesXY(uint64_t addr, int *x, int *y);

	/* highlighting stuff */
	uint32_t autoPalette[16] = { 
		0xffd966,0xd9ff66,0x8cff66,0x66cf8c,0xff8c66,0xffc929,0xebb000,0x66ffd9,
		0xff668c,0x003beb,0x295eff,0x66d9ff,0xff66d9,0xd966ff,0x8c66ff,0x668cff
	};
	int autoPaletteIdx = 0;
    void hlAdd(uint64_t left, uint64_t right, uint32_t color);
    void hlAdd(uint64_t left, uint64_t right);
    void hlClear(void);

    /* GUI geometry */
    int addrWidth=0;
    int addrWidth64=0;
    int addrWidth32=0;
    int byteWidth=0;
    int bytesWidth=0;
    int asciiWidth=0;
    int charWidth=0;
    int marginLeft, marginRight, marginTop, marginBottom;
    int viewWidth, viewHeight;
    int lineWidth, lineHeight;

	bool showAddress=true;
	bool showAscii=true;

    int bytesPerLine;
    int cursorOffs;
   
    /* highlight info */
    bool hlEnabled=false;
    IntervalMgr hlRanges;

    /* selection info */
    int selEditing=0, selActive=0;
    uint64_t addrSelStart, addrSelEnd;

    /* callback */
    HexView_callback callback=NULL;
};
