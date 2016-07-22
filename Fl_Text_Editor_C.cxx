/* FL_Text_Editor with C/C++ syntax highlighting (from fltk editor example) */

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

#include "Fl_Text_Editor_C.h"

/*****************************************************************************/
/* syntax stuff (can exist outside the class, takes only char *'s) */
/*****************************************************************************/
#define TS 14 // default editor textsize

Fl_Text_Buffer     *m_styleBuf = 0;

Fl_Text_Editor::Style_Table_Entry
styletable[] = {    // Style table
    { FL_BLACK,      FL_COURIER,           TS }, // A - Plain
    { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS }, // B - Line comments
    { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS }, // C - Block comments
    { FL_BLUE,       FL_COURIER,           TS }, // D - Strings
    { FL_DARK_RED,   FL_COURIER,           TS }, // E - Directives
    { FL_DARK_RED,   FL_COURIER_BOLD,      TS }, // F - Types
    { FL_BLUE,       FL_COURIER_BOLD,      TS }, // G - Keywords
};

// List of known C/C++ keywords...
// (sorted for bsearch())
const char *code_keywords[] = {
    "and", "and_eq", "asm", "bitand", "bitor", "break", "case", "catch", "compl",
    "continue", "default", "delete", "do", "else", "false", "for", "goto", "if",
    "new", "not", "not_eq", "operator", "or", "or_eq", "return", "switch",
    "template", "this", "throw", "true", "try", "while", "xor", "xor_eq"
};

// List of known C/C++ types...
// (sorted for bsearch())
const char *code_types[] = {
    "auto", "bool", "char", "class", "const", "const_cast", "double",
    "dynamic_cast", "enum", "explicit", "extern", "float", "friend", "inline",
    "int", "long", "mutable", "namespace", "private", "protected", "public",
    "register", "short", "signed", "sizeof", "static", "static_cast", "struct",
    "template", "typedef", "typename", "union", "unsigned", "virtual", "void",
    "volatile"
};

extern "C" /* so bswap will take it */
int
compare_keywords(const void *a, const void *b) 
{
    return (strcmp(*((const char **)a), *((const char **)b)));
}

// 'style_parse()' - Parse text and produce style data.
void
style_parse(const char *text, char *style, int length) {
    char current;
    int col;
    int last;
    char  buf[255], *bufptr;
    const char *temp;

    // Style letters:
    //
    // A - Plain
    // B - Line comments
    // C - Block comments
    // D - Strings
    // E - Directives
    // F - Types
    // G - Keywords

    for (current = *style, col = 0, last = 0; length > 0; length --, text ++) {
        if (current == 'B' || current == 'F' || current == 'G') current = 'A';
        if (current == 'A') {
            // Check for directives, comments, strings, and keywords...
            if (col == 0 && *text == '#') {
                current = 'E';
            } 
            else if (strncmp(text, "//", 2) == 0) {
                current = 'B';
                for (; length > 0 && *text != '\n'; length --, text ++) 
                    *style++ = 'B';
                if (length == 0) 
                    break;
            } 
            else if (strncmp(text, "/*", 2) == 0) {
                current = 'C';
            } 
            else if (strncmp(text, "\\\"", 2) == 0) {
                // Quoted quote...
                *style++ = current;
                *style++ = current;
                text ++;
                length --;
                col += 2;
                continue;
            } 
            else if (*text == '\"') {
                current = 'D';
            } 
            else if (!last && (islower((*text)&255) || *text == '_')) {
                // Might be a keyword...
                for (temp = text, bufptr = buf;
                    (islower((*temp)&255) || *temp == '_') && bufptr < (buf + sizeof(buf) - 1);
                    *bufptr++ = *temp++);

                if (!islower((*temp)&255) && *temp != '_') {
                    *bufptr = '\0';

                    bufptr = buf;

                    if (bsearch(&bufptr, code_types,
                                sizeof(code_types) / sizeof(code_types[0]),
                                sizeof(code_types[0]), compare_keywords)) {
                        while (text < temp) {
                            *style++ = 'F';
                            text ++;
                            length --;
                            col ++;
                        }

                        text --;
                        length ++;
                        last = 1;
                        continue;
                    } else if (bsearch(&bufptr, code_keywords,
                                sizeof(code_keywords) / sizeof(code_keywords[0]),
                                sizeof(code_keywords[0]), compare_keywords)) {
                        while (text < temp) {
                            *style++ = 'G';
                            text ++;
                            length --;
                            col ++;
                        }

                        text --;
                        length ++;
                        last = 1;
                        continue;
                    }
                }
            }
        } 
        else if (current == 'C' && strncmp(text, "*/", 2) == 0) {
            // Close a C comment...
            *style++ = current;
            *style++ = current;
            text ++;
            length --;
            current = 'A';
            col += 2;
            continue;
        } 
        else if (current == 'D') {
            // Continuing in string...
            if (strncmp(text, "\\\"", 2) == 0) {
                // Quoted end quote...
                *style++ = current;
                *style++ = current;
                text ++;
                length --;
                col += 2;
                continue;
            } else if (*text == '\"') {
                // End quote...
                *style++ = current;
                col ++;
                current = 'A';
                continue;
            }
        }

        // Copy style info...
        if (current == 'A' && (*text == '{' || *text == '}')) *style++ = 'G';
        else *style++ = current;
        col++;

        last = isalnum((*text)&255) || *text == '_' || *text == '.';

        if (*text == '\n') {
            // Reset column and possibly reset the style
            col = 0;
            if (current == 'B' || current == 'E') current = 'A';
        }
    }
}

// 'style_unfinished_cb()' - Update unfinished styles.
void
style_unfinished_cb(int, void*)
{
    return;
}

void
source_modified_cb(int a, int b, int c, int d, const char *e, void *cbArg)
{
    Fl_Text_Editor_C *widget = (Fl_Text_Editor_C *)cbArg;
    widget->onSrcMod(a, b, c, d, e, cbArg);
}

/*****************************************************************************/
/* class stuff */
/*****************************************************************************/

Fl_Text_Editor_C::Fl_Text_Editor_C(int x, int y, int w, int h, const char* label):
  Fl_Text_Editor(x, y, w, h, label) {

    m_styleBuf = new Fl_Text_Buffer();
}

/* we intercept any replacement of the buffer to allocate a parallel style
    buffer and add our style callback */
void Fl_Text_Editor_C::buffer(Fl_Text_Buffer *buf)
{
    Fl_Text_Display::buffer(buf);
    styleRealloc();
    buf->add_modify_callback(source_modified_cb, this);
    Fl_Text_Editor::highlight_data(m_styleBuf, styletable, 7, 'X', NULL, NULL);
}

void Fl_Text_Editor_C::buffer(Fl_Text_Buffer &buf)
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
void Fl_Text_Editor_C::onSrcMod(int pos,        // Position of update
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

    free(text);
    free(style);
}

void Fl_Text_Editor_C::styleRealloc() {
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

