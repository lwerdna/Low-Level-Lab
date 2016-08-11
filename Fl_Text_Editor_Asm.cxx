/* FL_Text_Editor with assembly syntax highlighting */

#include <string>
#include <cctype>
#include <locale>
using namespace std;

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
    { FL_DARK_GREEN, FL_HELVETICA,         TS }, // B - Line comments
    { FL_DARK_BLUE,  FL_COURIER,           TS }, // C - numbers
    { FL_DARK_CYAN,  FL_COURIER_BOLD,      TS }, // D - Labels
    { FL_DARK_RED,   FL_COURIER,           TS }, // E - Directives
    { FL_DARK_RED,   FL_COURIER,           TS }, // F - Registers
    { FL_BLUE,       FL_COURIER_BOLD,      TS }, // G - Keywords
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

/*****************************************************************************/
/* X86 style parsing (at&t) */
/*****************************************************************************/
// (sorted for bsearch())
const char *x86_regs_att[] = {
    "%eax", "%ebp", "%ebx", "%ecx", "%edi", "%edx", "%esi",
    "%rax", "%rbp", "%rbx", "%rcx", "%rdi", "%rdx", "%rip", "%rsi", "%rsp"
};

extern "C" { /* so bswap will take it */
static
int
compare_x86_regs_att(const void *a_, const void *b_) 
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
style_parse_x86_att(const char *text, char *style, int length) 
{
    for(int i=0; i<length; ) {
        bool boring = true;

        /* pick out registers */
        if(length-i >= 4 && text[0] != ' ') {
            if(bsearch(text+i, &(x86_regs_att[0]), ARRAY_LEN(x86_regs_att), sizeof(char *), compare_x86_regs_att)) {
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
            const char *b = strstr(text+i, "\n");
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

static
void
style_parse_x86_intel(const char *text, char *style, int length) 
{
    for(int i=0; i<length; ) {
        bool boring = true;

        /* pick out registers */
        if(length-i >= 4 && text[0] != ' ') {
            if(bsearch(text+i, &(x86_regs_att[0]), ARRAY_LEN(x86_regs_att), sizeof(char *), compare_x86_regs_att)) {
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
            const char *b = strstr(text+i, "\n");
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
    m_x86_flavor = X86_FLAVOR_INTEL;
    m_autoSyntaxHighlight = 1;
    m_styleFunc = style_parse_x86_intel;
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

void Fl_Text_Editor_Asm::synHighlightNow()
{
    styleRealloc();

    Fl_Text_Buffer *source = Fl_Text_Display::buffer();
    // innefficient to do this everytime, but who cares for now
    styleRealloc();
}

// this is the callback when the source buffer is modified
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
    /* get the text buffer we are enveloping */
    Fl_Text_Buffer *source = Fl_Text_Display::buffer();

    if (nInserted == 0 && nDeleted == 0) {
        printf("none inserted, none deleted, just unselecting the style buff\n");
        m_styleBuf->unselect();
        return;
    }

    if(m_autoSyntaxHighlight) {
        synHighlightNow();
    };
}

void Fl_Text_Editor_Asm::styleRealloc() 
{
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
        m_styleFunc(source, style, ftb->length());
        free(source);

        // set the m_styleBuf to the style bytes
        m_styleBuf->text(style);
        delete[] style;
    }
}

void Fl_Text_Editor_Asm::styleSet(int style)
{
    switch(style) {
        case STYLE_X86_ATT:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_INTEL:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_64_ATT:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_64_INTEL:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_ARM:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_THUMB:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_AARCH64:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_PPC:
            m_styleFunc = style_parse_x86_intel;
            break;
        case STYLE_X86_PPC64:
            m_styleFunc = style_parse_x86_intel;
            break;
        default:
            printf("ERROR: unknown style id: %d\n", style);
    }
}

