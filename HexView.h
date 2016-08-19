#pragma once

#include <map>
#include <vector>
using namespace std;

#include <FL/Fl_Widget.H>

#include "IntervalMgr.h"

using namespace std;

class HexView : public Fl_Widget {
    public:

    HexView(int X, int Y, int W, int H, const char *label=0);

    void setBytes(uint64_t addr, uint8_t *bytes, int len);
    void setView(uint64_t addr);
    void setView();
    void clearBytes(void);
    int handle(int event);
    void draw();
    void resize(int, int, int, int);

    /* live view information */
    int linesPerPage;
    int bytesPerPage;
    int linesInView;
    int bytesInView;
    uint64_t addrViewStart, addrViewEnd; // [,)

    int viewAddrToBytesXY(uint64_t addr, int *x, int *y);
    int viewAddrToAsciiXY(uint64_t addr, int *x, int *y);
    int viewOffsToBytesXY(int offset, int *x, int *y);
    int viewOffsToAsciiXY(int offset, int *x, int *y);

    void hlAdd(uint64_t left, uint64_t right, uint32_t color);

    private:
    int addrMode; // 32 or 64

    /* the entire memory buffer */
    uint64_t addrStart, addrEnd;
    uint8_t *bytes;
    int nBytes;

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

    int bytesPerLine;
    int cursorOffs;
   
    /* highlight info */
    IntervalMgr hlRanges;

    /* selection info */
    int selEditing=0, selActive=0;
    uint64_t selAddrStart, selAddrEnd;
};
