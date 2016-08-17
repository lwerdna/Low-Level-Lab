#pragma once

#include <map>
#include <vector>
using namespace std;

#include <FL/Fl_Widget.H>


using namespace std;

class HexView : public Fl_Widget {
    public:

    HexView(int X, int Y, int W, int H, const char *label=0);

    void setBytes(uint64_t addr, uint8_t *bytes, int len);
    void clearBytes(void);
    int handle(int event);
    void draw();
    void resize(int, int, int, int);

    //int getCursorLoc(int *loc);
    //int getSelection(uint64_t &addrStart, uint64_t &addrEnd);

    int getNumLines();
    int getNumBytesDisplayable();

    private:
    int addrMode; // 32 or 64

    /* address label for the start of the buffer, of the end of buffer, and
        of the current view */
    uint64_t addrStart, addrEnd, addrView;

    uint8_t *bytes;
    int nBytes;
    int addrWidth;
    int addrWidth64;
    int addrWidth32;
    int byteWidth;
    int bytesWidth;
    int asciiWidth;
    int charWidth;
    int marginLeft, marginRight, marginTop, marginBottom;
    int viewWidth, viewHeight;
    int lineWidth, lineHeight;

    int bytesPerLine;
    
    vector<uint64_t> hlStarts;
    map<uint64_t,uint64_t> hlStartToEnd;
    map<uint64_t,uint64_t> hlStartToColor;

    uint64_t selAddrStart, selAddrEnd;
};
