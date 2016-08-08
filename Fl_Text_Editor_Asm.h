#pragma once

#include <FL/Fl_Text_Editor.h>

#define X86_FLAVOR_ATT 0
#define X86_FLAVOR_INTEL 1

#define STYLE_X86_ATT 0
#define STYLE_X86_INTEL 1
#define STYLE_X86_64_ATT 2
#define STYLE_X86_64_INTEL 3
#define STYLE_X86_ARM 4
#define STYLE_X86_THUMB 5
#define STYLE_X86_AARCH64 6
#define STYLE_X86_PPC 7
#define STYLE_X86_PPC64 8

class Fl_Text_Editor_Asm : public Fl_Text_Editor {
    private:
        int m_x86_flavor;
        int m_autoSyntaxHighlight;
        void (*m_styleFunc)(const char *text, char *style, int length);
        Fl_Text_Buffer *m_styleBuf;

    public:
        Fl_Text_Editor_Asm(int x, int y, int w, int h);

        // things we override
        void buffer(Fl_Text_Buffer *buf);
        void buffer(Fl_Text_Buffer &buf);

        // buffer callback 
        void onSrcMod(int, int, int, int, const char *, void *);

        // 
        void synHighlightAutomatically(void);
        void synHighlightWhenTold(void);
        void synHighlightNow(void);

        //
        void styleSet(int style);

        //
        void styleRealloc(void);
};

