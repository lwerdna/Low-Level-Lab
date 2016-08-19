/* c stdlib includes */
#include <time.h>
#include <stdio.h>

/* OS */
#include <sys/mman.h>

/* c++ includes */
#include <map>
#include <string>
#include <vector>
using namespace std;

#include "rsrc.h"
#include "HlabGui.h"

/* fltk includes */
#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>

/* autils */
extern "C" {
}

/* globals */
HlabGui *gui = NULL;

FILE *fileOpenFp = NULL;
void *fileOpenPtrMap = NULL;
size_t fileOpenSize = 0;

Fl_Window *winTags = NULL;

const char *initStr = "This is the default bytes when no file is open. Here's some deadbeef: \xDE\xAD\xBE\xEF";

int close_current_file(void)
{
    int rc = -1;

    if(fileOpenPtrMap && fileOpenSize) {
        munmap(fileOpenPtrMap, fileOpenSize);
        fileOpenPtrMap = NULL;
        fileOpenSize = 0;
    }

    if(fileOpenFp) {
        fclose(fileOpenFp);
        fileOpenFp = NULL;
    }

    if(!fileOpenFp && !fileOpenPtrMap && !fileOpenSize) {
        rc = 0;
    }

    return rc;
}

/*****************************************************************************/
/* MENU CALLBACKS */
/*****************************************************************************/

void open_cb(Fl_Widget *, void *)
{
    int rc = -1;

    Fl_File_Chooser chooser(
        ".",    // directory
        "*",    // filter
        Fl_File_Chooser::MULTI, // type
        "Edit File"             // title
    );

    chooser.show();

    while(chooser.shown()) {
        Fl::wait();
    }

    if(chooser.value() == NULL) {
        return;
    }

    printf("--------------------\n");
    printf("DIRECTORY: '%s'\n", chooser.directory());
    printf("    VALUE: '%s'\n", chooser.value());
    printf("    COUNT: %d files selected\n", chooser.count());

    if(0 != close_current_file()) {
        printf("ERROR: close_current_file()\n");
        goto cleanup;
    }

    fileOpenFp = fopen(chooser.value(), "rb");
    if(!fileOpenFp) {
        printf("ERROR: fopen()\n");
        goto cleanup;
    }

    fseek(fileOpenFp, 0, SEEK_END);
    fileOpenSize = ftell(fileOpenFp);
    fseek(fileOpenFp, 0, SEEK_SET);

    fileOpenPtrMap = mmap(0, fileOpenSize, PROT_READ, MAP_PRIVATE, fileno(fileOpenFp), 0);
    if(fileOpenPtrMap == MAP_FAILED) {
        printf("ERROR: mmap()\n");
        goto cleanup;
    }

    gui->hexView->setBytes(0, (uint8_t *)fileOpenPtrMap, fileOpenSize);

    rc = 0;
    cleanup:

    if(rc) {
        if(0 != close_current_file()) {
            printf("ERROR: close_current_file()\n");
            goto cleanup;
        }
    }

    return;
}

void new_cb(Fl_Widget *, void *) {
    return;
}

void open_view(Fl_Widget *, void *) {
    return;
}

void insert_cb(Fl_Widget *, void *) {
    return;
}

void save_cb(Fl_Widget *, void *) {
    return;
}

void saveas_cb(Fl_Widget *, void *) {
    return;
}

void close_cb(Fl_Widget *, void *) {
   gui->hexView->setBytes(0, (uint8_t *)initStr, strlen(initStr));
   close_current_file();
   return;
}

void quit_cb(Fl_Widget *, void *) {
   gui->hexView->setBytes(0, (uint8_t *)initStr, strlen(initStr));
   close_current_file();
   return;
   if(fileOpenFp) {
        fclose(fileOpenFp);
        fileOpenFp = NULL;
    }
    gui->mainWindow->hide();
    return;
}

void cut_cb(Fl_Widget *, void *) {
    return;
}

void copy_cb(Fl_Widget *, void *) {
    return;
}

void paste_cb(Fl_Widget *, void *) {
    return;
}

void delete_cb(Fl_Widget *, void *) {
    return;
}

void find_cb(Fl_Widget *, void *) {
    return;
}

void find2_cb(Fl_Widget *, void *) {
    return;
}

void replace_cb(Fl_Widget *, void *) {
    return;
}

void replace2_cb(Fl_Widget *, void *) {
    return;
}

void tags_cb(Fl_Widget *widg, void *ptr)
{
    Fl_Tree *tree;

    if(!winTags) {
        winTags = new Fl_Window(gui->mainWindow->x()+gui->mainWindow->w(), gui->mainWindow->y(), gui->mainWindow->w()/2, gui->mainWindow->h(), "tags");
        tree = new Fl_Tree(0, 0, winTags->w(), winTags->h());
        tree->end();
        tree->clear();
        Fl_Tree_Item *i = tree->add("one");
        tree->add(i, "one_sub_0");
        tree->add(i, "one_sub_1");
        tree->add(i, "one_sub_2");
        tree->add("two");
        tree->add("three");
        tree->add("mystruct");
        tree->add("mystruct/memberA");
        tree->add("mystruct/memberB");
        tree->add("mystruct/memberC");
        tree->add("mystruct/memberA/fieldA");
        tree->add("mystruct/memberA/fieldB");
        tree->add("mystruct/memberA/fieldC");
        tree->add("mystruct/memberA/fieldA", NULL);
        tree->add("mystruct/memberA/fieldB", NULL);
        tree->add("mystruct/memberA/fieldC", NULL);
        winTags->end();
        winTags->resizable(tree);
    }
        
    winTags->show();

    return;
}

/*****************************************************************************/
/* GUI CALLBACK STUFF */
/*****************************************************************************/

void
onGuiFinished(HlabGui *gui_)
{
    printf("%s()\n", __func__);
    int rc = -1;

    gui = gui_;

    /* menu bar */
    static Fl_Menu_Item menuItems[] = {
        { "&File",              0, 0, 0, FL_SUBMENU },
//        { "&New File",        0, (Fl_Callback *)new_cb },
        { "&Open",    FL_COMMAND + 'o', (Fl_Callback *)open_cb },
//        { "&Insert File...",  FL_COMMAND + 'i', (Fl_Callback *)insert_cb, 0, FL_MENU_DIVIDER },
//        { "&Save File",       FL_COMMAND + 's', (Fl_Callback *)save_cb },
//        { "Save File &As...", FL_COMMAND + FL_SHIFT + 's', (Fl_Callback *)saveas_cb, 0, FL_MENU_DIVIDER },
        { "&Close",           FL_COMMAND + 'w', (Fl_Callback *)close_cb, 0, FL_MENU_DIVIDER },
        { "E&xit",            FL_COMMAND + 'q', (Fl_Callback *)quit_cb, 0 },
        { 0 },

        { "&Tags", FL_COMMAND, (Fl_Callback *)tags_cb },
        { 0 },

//        { "&Edit", 0, 0, 0, FL_SUBMENU },
//        { "Cu&t",             FL_COMMAND + 'x', (Fl_Callback *)cut_cb },
//        { "&Copy",            FL_COMMAND + 'c', (Fl_Callback *)copy_cb },
//        { "&Paste",           FL_COMMAND + 'v', (Fl_Callback *)paste_cb },
//        { "&Delete",          0, (Fl_Callback *)delete_cb },
//        { 0 },

//        { "&Search", 0, 0, 0, FL_SUBMENU },
//        { "&Find...",         FL_COMMAND + 'f', (Fl_Callback *)find_cb },
//        { "F&ind Again",      FL_COMMAND + 'g', find2_cb },
//        { "&Replace...",      FL_COMMAND + 'r', replace_cb },
//        { "Re&place Again",   FL_COMMAND + 't', replace2_cb },
//        { 0 },

        { 0 }
    };
    
    gui->menuBar->copy(menuItems);
    
    gui->hexView->setBytes(0, (uint8_t *)initStr, strlen(initStr));

    /* test some colors */
    gui->hexView->hlAdd(3,8,  0xFF000000);
    gui->hexView->hlAdd(8,12, 0x0FF00000);
    gui->hexView->hlAdd(12,15,0x00FF0000);
    gui->hexView->hlAdd(20,30,0x000FF000);
    gui->hexView->hlAdd(35,40,0x0000FF00);
    gui->hexView->hlAdd(50,90,0x00000FF0);
    gui->hexView->hlAdd(100,200,0x000000FF);

    rc = 0;
    //cleanup:
    return;
}

void
onIdle(void *data)
{
    
}

void
onExit(void)
{
    printf("onExit()\n");
}
