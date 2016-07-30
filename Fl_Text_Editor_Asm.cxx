/* FL_Text_Editor with assembly syntax highlighting */

#include <string.h>

#include <FL/Fl.H>
#include <FL/x.H> // for fl_open_callback
#include <FL/Fl_Group.H>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/filename.H>

#include "Fl_Text_Editor_Asm.h"


/*****************************************************************************/
/* syntax stuff (can exist outside the class, takes only char *'s) */
/*****************************************************************************/
#define TS 14 // default editor textsize

static
Fl_Text_Editor::Style_Table_Entry
styletable[] = {    // Style table
    { FL_BLACK,      FL_COURIER,           TS }, // A - Plain
    { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS }, // B - Line comments
    { FL_DARK_BLUE,  FL_COURIER,           TS }, // C - numbers
    { FL_DARK_CYAN,  FL_COURIER_BOLD,      TS }, // D - Labels
    { FL_DARK_RED,   FL_COURIER,           TS }, // E - Directives
    { FL_DARK_RED,   FL_COURIER,           TS }, // F - Registers
    { FL_BLUE,       FL_COURIER_BOLD,      TS }, // G - Keywords
};

// List of known C/C++ types...
// (sorted for bsearch())

const char *x86_regs[] = {
    "%eax", "%ebp", "%ebx", "%ecx", "%edi", "%edx", "%esi",
    "%rax", "%rbp", "%rbx", "%rcx", "%rdi", "%rdx", "%rip", "%rsi", "%rsp"
};

//const char *x86_mnemonics[] = {
//
//};

#define ARRAY_LEN(X) (sizeof(X)/sizeof(*X))

extern "C" { /* so bswap will take it */
static
int
compare_keywords(const void *a, const void *b) 
{
    return (strcmp(*((const char **)a), *((const char **)b)));
}
}

extern "C" { /* so bswap will take it */
static
int
compare_x86_regs(const void *a_, const void *b_) 
{
    char *a = (char *)a_;
    char *b = *(char **)b_;
    //printf("comparing -%c%c%c%c- to -%c%c%c%c-\n",
    //    a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);

    return strncmp((const char *)a, (const char *)b, 4);
}
}

// 'style_parse()' - Parse text and produce style data.
static
void
style_parse(const char *text, char *style, int length) 
{
    for(int i=0; i<length; ) {
        bool boring = true;

        /* pick out registers */
        if(length-i >= 4 && text[0] != ' ') {
            if(bsearch(text+i, &(x86_regs[0]), ARRAY_LEN(x86_regs), sizeof(char *), compare_x86_regs)) {
                strncpy(style+i, "FFFF", 4);
                i += 4;
                boring = false;
            }
        }

        /* pick out numbers */
        if(boring && length-i >= 3 && text[i]=='0' && text[i+1]=='x' && isxdigit(text[i+2])) {
            int numLen = 3;
            while(isxdigit(text[i+numLen++]));
            numLen -= 1;
            memset(style+i, 'C', numLen);
            i += numLen;
            boring = false;
        }


        /* pick out line comments */
        if(boring && text[i]=='#') {
            int nlLen = strlen("\n");
            /* end-to-line comment, seek to newline or end of buffer */
            int commLen = 1;
            for(int j=i+1; j<length; ++j)
                if(0==strncmp(text+j, "\n", nlLen))
                    break;
                else
                    commLen++;
            memset(style+i, 'B', commLen);
            i += commLen;
            boring = false;
        }

        /* pick out labels */
        if(boring && (length-i >= 3) && text[i] != ' ') {
            char *b = strstr(text+i, "\n");
            if(b && b!=(text+i) && b[-1] == ':') {
                int labelLen = b - (text+i);
                //printf("labelLen is %d\n", labelLen);
                memset(style+i, 'D', labelLen);
                i += labelLen;
                boring = false;
            }
        }

        if(boring) {
            style[i++] = 'A';
        }
    }
}

// 'style_unfinished_cb()' - Update unfinished styles.
static
void
style_unfinished_cb(int, void*)
{
    return;
}

static
void
source_modified_cb(int a, int b, int c, int d, const char *e, void *cbArg)
{
    Fl_Text_Editor_Asm *widget = (Fl_Text_Editor_Asm *)cbArg;
    widget->onSrcMod(a, b, c, d, e, cbArg);
}

/*****************************************************************************/
/* class stuff */
/*****************************************************************************/

Fl_Text_Editor_Asm::Fl_Text_Editor_Asm(int x, int y, int w, int h):
  Fl_Text_Editor(x, y, w, h) {

    m_styleBuf = new Fl_Text_Buffer();
}

/* we intercept any replacement of the buffer to allocate a parallel style
    buffer and add our style callback */
void Fl_Text_Editor_Asm::buffer(Fl_Text_Buffer *buf)
{
    Fl_Text_Display::buffer(buf);
    styleRealloc();
    buf->add_modify_callback(source_modified_cb, this);
    Fl_Text_Editor::highlight_data(m_styleBuf, styletable, 7, 'X', NULL, NULL);
}

void Fl_Text_Editor_Asm::buffer(Fl_Text_Buffer &buf)
{
    Fl_Text_Display::buffer(buf);
    styleRealloc();
    buf.add_modify_callback(source_modified_cb, this);
    Fl_Text_Editor::highlight_data(m_styleBuf, styletable, 7, 'X', NULL, NULL);
}

// this is the callback when the c source buffer is modified
//
// typedef void (*Fl_Text_Modify_Cb)(int pos, int nInserted, int nDeleted, 
//   int nRestyled, const char* deletedText, void* cbArg);
//
void Fl_Text_Editor_Asm::onSrcMod(int pos,        // Position of update
        int nInserted,    // Number of inserted chars
        int nDeleted,    // Number of deleted chars
        int nRestyled,    // Number of restyled chars
        const char *deletedText, // Text that was deleted
        void *cbArg)     // Callback data
  {
    int    start,                // Start of text
    end;                // End of text
    char    last, *style, *text;                // Last style on line

    Fl_Text_Buffer *source = Fl_Text_Display::buffer();

    // If this is just a selection change, just unselect the style buffer...
    if (nInserted == 0 && nDeleted == 0) {
        m_styleBuf->unselect();
        return;
    }

    // Track changes in the text buffer...
    if (nInserted > 0) {
        // Insert characters into the style buffer...
        style = new char[nInserted + 1];
        memset(style, 'A', nInserted);
        style[nInserted] = '\0';

        m_styleBuf->replace(pos, pos + nDeleted, style);
        delete[] style;
    } 
    else {
        // Just delete characters in the style buffer...
        m_styleBuf->remove(pos, pos + nDeleted);
    }

    // Select the area that was just updated to avoid unnecessary
    // callbacks...
    m_styleBuf->select(pos, pos + nInserted - nDeleted);

    // Re-parse the changed region; we do this by parsing from the
    // beginning of the previous line of the changed region to the end of
    // the line of the changed region...  Then we check the last
    // style character and keep updating if we have a multi-line
    // comment character...
    start = source->line_start(pos);
    //  if (start > 0) start = source->line_start(start - 1);
    end   = source->line_end(pos + nInserted);
    text  = source->text_range(start, end);
    style = m_styleBuf->text_range(start, end);
    if (start==end)
        last = 0;
    else
        last  = style[end - start - 1];

    //  printf("start = %d, end = %d, text = \"%s\", style = \"%s\", last='%c'...\n",
    //         start, end, text, style, last);

    style_parse(text, style, end - start);

    //  printf("new style = \"%s\", new last='%c'...\n",
    //         style, style[end - start - 1]);

    m_styleBuf->replace(start, end, style);
    ((Fl_Text_Editor *)cbArg)->redisplay_range(start, end);

    if (start==end || last != style[end - start - 1]) {
        //    printf("Recalculate the rest of the buffer style\n");
        // Either the user deleted some text, or the last character
        // on the line changed styles, so reparse the
        // remainder of the buffer...
        free(text);
        free(style);

        end   = source->length();
        text  = source->text_range(start, end);
        style = m_styleBuf->text_range(start, end);

        style_parse(text, style, end - start);

        m_styleBuf->replace(start, end, style);
        ((Fl_Text_Editor *)cbArg)->redisplay_range(start, end);
    }

    printf("got to the end of style parsing\n");

    free(text);
    free(style);
}

void Fl_Text_Editor_Asm::styleRealloc() {
    Fl_Text_Buffer *ftb = Fl_Text_Display::buffer();

    if(!ftb) {
        printf("ERROR: no buffer set!?\n");
    }
    else {
        // allocate style bytes
        char *style = new char[ftb->length() + 1];

        // fill style bytes with first style entry
        memset(style, 'A', ftb->length());
        style[ftb->length()] = '\0';
   
        // alloc a char* for non-oop style function
        char *source = ftb->text();
        style_parse(source, style, ftb->length());
        free(source);

        // set the m_styleBuf to the style bytes
        m_styleBuf->text(style);
        delete[] style;
    }
}

