#pragma once

#include <FL/Fl_Text_Editor.h>

class Fl_Text_Editor_Asm : public Fl_Text_Editor {
    private:
        Fl_Text_Buffer *m_styleBuf;

    public:
        Fl_Text_Editor_Asm(int x, int y, int w, int h);

        // things we override
        void buffer(Fl_Text_Buffer *buf);
        void buffer(Fl_Text_Buffer &buf);

        // buffer callback 
        void onSrcMod(int, int, int, int, const char *, void *);

        //
        void styleRealloc(void);
};

