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
    #include <autils/bytes.h>
}

/* forward dec's */
void tree_cb(Fl_Tree *, void *);

/* globals */
HlabGui *gui = NULL;

FILE *fileOpenFp = NULL;
void *fileOpenPtrMap = NULL;
size_t fileOpenSize = 0;

Fl_Window *winTags = NULL;
Fl_Tree *tree = NULL;
map<Fl_Tree_Item *, Interval *> treeItemToInterv;
IntervalMgr intervMgr;

const char *initStr = "This is the default bytes when no file is open. Here's some deadbeef: \xDE\xAD\xBE\xEF";

int file_unload(void)
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

int file_load(const char *path) 
{
    int rc = -1;

    if(fileOpenFp)
        file_unload();

    fileOpenFp = fopen(path, "rb");
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
        if(0 != file_unload()) {
            printf("ERROR: file_unload()\n");
        }
    }

    return rc;
}

/*****************************************************************************/
/* FILE READ TAGS */
/*****************************************************************************/

void populateTree(Fl_Tree *tree, Fl_Tree_Item *parentItem, Interval *parentIval)
{
    /* add this dude straight away */
    int pos = parentItem->children();
    Fl_Tree_Item *childItem = tree->insert(parentItem, parentIval->data_string.c_str(), pos);

    treeItemToInterv[childItem] = parentIval;
    //printf("child ITEM %p maps to INTERVAL %p\n", childItem, parentIval);

    /* recur on each child */
    vector<Interval *> childrenIval = parentIval->childRetrieve();

    for(int i=0; i<childrenIval.size(); ++i) {
        Interval *childIval = childrenIval[i];
        populateTree(tree, childItem, childIval);
    }
}

int load_tags(const char *fpath)
{
    int rc = -1;
    int line_num = 1;
    char *line = NULL;

    bool bOldHighlightsCleared = false;

    FILE *fp = fopen(fpath, "r");
    if(!fp) {
        printf("ERROR: fopen()\n");
        goto cleanup;
    }

    intervMgr.clear();

    for(int line_num=1; 1; ++line_num) {
        size_t line_len;

        if(line) {
            free(line);
            line = NULL;
        }
        if(getline(&line, &line_len, fp) <= 0) {
            // error or EOF? don't squawk
            break;
        }

        if(0 == strcmp(line, "\x0d\x0a") || 0 == strcmp(line, "\x0a")) {
            continue;
        }

        /* remove trailing newline */
        while(line[line_len-1] == '\x0d' || line[line_len-1] == '\x0a') {
            line[line_len-1] = '\x00';
            line_len -= 1;
        }

        //printf("got line %d (len:%ld): -%s-", line_num, line_len, line);

        uint64_t start, end;
        uint32_t color;
        int oStart=-1, oEnd=-1, oColor=-1, oComment=-1;
        if(line[0] != '[') {
            printf("ERROR: expected '[' on line %d: %s\n", line_num, line);
            continue;
        }

        oStart = 1;
        /* seek comma (ending the start address) */
        for(int i=oStart; i<line_len; ++i) {
            if(line[i]==',') {
                line[i]='\x00';
                oEnd = i+1;
                break;
            }
        }

        if(oEnd == -1 || oEnd >= (line_len-1)) {
            printf("ERROR: missing or bad comma on line %d: %s\n", line_num, line);
            continue;
        }

        if(0 != parse_uint64_hex(line + oStart, &start)) {
            printf("ERROR: couldn't parse start address on line %d: %s\n", line_num, line);
            continue;
        }

        /* seek ') ' (ending the end address) */
        for(int i=oEnd; i<line_len; ++i) {
            if(0 == strncmp(line + i, ") ", 2)) {
                line[i]='\x00';
                oColor = i+2;
                break;
            }
        }

        if(oColor == -1 || oColor >= (line_len-1)) {
            printf("ERROR: missing or bad \") \" on line %d: %s\n", line_num, line);
            continue;
        }

        if(0 != parse_uint64_hex(line + oEnd, &end)) {
            printf("ERROR: couldn't parse end address on line %d: %s\n", line_num, line);
            continue;
        }
       
        /* seek next space or null (ending the color) */
        for(int i=oColor; 1; ++i) {
            if(0 == strncmp(line + i, " ", 1) || line[i]=='\x00') {
                line[i]='\x00';
                oComment = i+1;
                break;
            }
        }

        if(oComment >= line_len || strlen(line+oComment) == 0) {
            // missing comment, run it into null byte
            oComment = oColor-1;
        }

        if(0 != parse_uint32_hex(line + oColor, &color)) {
            printf("ERROR: couldn't parse color address on line %d: %s\n", line_num, line);
            continue;
        }

        /* done, add interval */
        intervMgr.add(Interval(start, end, string(line + oComment)));
    }

    /* add new tree item to the tree window */
    if(intervMgr.size()) {
        winTags = new Fl_Window(
            gui->mainWindow->x()+gui->mainWindow->w()+32, 
            gui->mainWindow->y(), gui->mainWindow->w(), 
            gui->mainWindow->h(), 
            "tags"
        );
        tree = new Fl_Tree(0, 0, winTags->w(), winTags->h());
        tree->end();
        tree->callback((Fl_Callback *)tree_cb);
        winTags->end();
        winTags->resizable(tree);

        tree->root_label("file");
        Fl_Tree_Item *rootItem = tree->root();

        vector<Interval *> rootsIval = intervMgr.findParentChild();
        for(int i=0; i<rootsIval.size(); ++i) {
            rootsIval[i]->print();
            populateTree(tree, rootItem, rootsIval[i]);
        }

        /* close all tree items except root */
        for(Fl_Tree_Item *item = tree->first(); item; item = tree->next(item)) {
            if (!item->is_root() && item->has_children()) {
                item->close();
            }
        }

        /* show window */
        winTags->show();
    }

    rc = 0;
    cleanup:

    /* this free must occur even if getline() failed */
    if(line) {
        free(line);
        line = NULL;
    }

    if(fp) {
        fclose(fp);
        fp = NULL;
    }

    return rc;
}

/*****************************************************************************/
/* HEXVIEW CALLBACK */
/*****************************************************************************/

void HexView_cb(int type, void *data)
{
    char msg[256] = {0};
    uint64_t tmp;
    HexView *hv = gui->hexView;

    char strAddrSelStart[16];
    char strAddrSelEnd[16];
    char strAddrViewStart[16];
    char strAddrViewEnd[16];
    char strAddrStart[16];
    char strAddrEnd[16];

    if(hv->addrMode == 64) {
        sprintf(strAddrSelStart, "0x%016llX", hv->addrSelStart);
        sprintf(strAddrSelEnd, "0x%016llX", hv->addrSelEnd);
        sprintf(strAddrViewStart, "0x%016llX", hv->addrViewStart);
        sprintf(strAddrViewEnd, "0x%016llX", hv->addrViewEnd);
        sprintf(strAddrStart, "0x%016llX", hv->addrStart);
        sprintf(strAddrEnd, "0x%016llX", hv->addrEnd);
    }
    else {
        sprintf(strAddrSelStart, "0x%08llX", hv->addrSelStart);
        sprintf(strAddrSelEnd, "0x%08llX", hv->addrSelEnd);
        sprintf(strAddrViewStart, "0x%08llX", hv->addrViewStart);
        sprintf(strAddrViewEnd, "0x%08llX", hv->addrViewEnd);
        sprintf(strAddrStart, "0x%08llX", hv->addrStart);
        sprintf(strAddrEnd, "0x%08llX", hv->addrEnd);
    }

    switch(type) {
        case HV_CB_SELECTION:
            if(hv->selActive) {
                tmp = hv->addrSelEnd - hv->addrSelStart;
                sprintf(msg, "selected 0x%llX (%lld) bytes [%s,%s)",
                    tmp, tmp, strAddrSelStart, strAddrSelEnd);
            }
            else {
                sprintf(msg, "selection cleared");
            }
            break;

        case HV_CB_VIEW_MOVE:
            sprintf(msg, "view moved [%s,%s) %.1f%%",
                strAddrViewStart, strAddrViewEnd, hv->viewPercent);
            break;
            
        case HV_CB_NEW_BYTES:
            sprintf(msg, "0x%X (%d) bytes to [%s,%s)",
                hv->nBytes, hv->nBytes, strAddrStart, strAddrEnd);
            break;
    }

    if(msg[0]) {
        gui->statusBar->value(msg);
    }
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

    //printf("--------------------\n");
    //printf("DIRECTORY: '%s'\n", chooser.directory());
    //printf("    VALUE: '%s'\n", chooser.value());
    //printf("    COUNT: %d files selected\n", chooser.count());

    file_load(chooser.value());

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
   file_unload();
   return;
}

void quit_cb(Fl_Widget *, void *) {
    file_unload();
    gui->mainWindow->hide();
    if(winTags) winTags->hide();
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

    load_tags(chooser.value());

    return;
}

/*****************************************************************************/
/* TREE CALLBACK */
/*****************************************************************************/
void tree_cb(Fl_Tree *, void *)
{
    switch(tree->callback_reason()) {
        case FL_TREE_REASON_NONE:
            //printf("tree, no reason\n");
            break;
        case FL_TREE_REASON_SELECTED: 
        {
            Fl_Tree_Item *item = tree->callback_item();
            //printf("tree item %p clicked!\n", item);
            if(treeItemToInterv.find(item) == treeItemToInterv.end()) {
                //printf("not found though!\n");
            }
            else { 
                Interval *ival = treeItemToInterv[item];
                //printf("interval @%p has [%016llX,%016llX]\n", 
                //    ival, ival->left, ival->right);

                /* move upwards along the tree, coloring */
                //uint32_t palette[5] = {0xece2a5, 0xa5cebc, 0x768aa5, 0x496376, 0xa9212e};
                uint32_t palette[5] = {0xFFFF00, 0x0000FF, 0x088000, 0x880000, 0xFF0000};

                vector<Fl_Tree_Item *> lineage;
    
                Fl_Tree_Item *curr = item;
                while(curr) {
                    lineage.insert(lineage.begin(), curr);
                    curr = curr->parent();
                }
                
                gui->hexView->hlClear();
                for(int i=0; i<lineage.size(); ++i) {
                    if(treeItemToInterv.find(lineage[i]) == treeItemToInterv.end()) {
                        //printf("not found though!\n");
                    }
                    else {
                        Interval *ival = treeItemToInterv[lineage[i]];
                        if(ival) {
                            gui->hexView->hlAdd(ival->left, ival->right, palette[i % 5]);
                        }
                    }
                }

                gui->hexView->setView((ival->left > 0x40) ? ival->left - 0x40 : 0);
                gui->hexView->setSelection(ival->left, ival->right);
            }
        }
        case FL_TREE_REASON_DESELECTED: 
            //printf("tree, deselected\n");
            break;
        case FL_TREE_REASON_OPENED: 
            //printf("tree, opened\n");
            break;
        case FL_TREE_REASON_CLOSED: 
            //printf("tree, closed\n");
            break;
        case FL_TREE_REASON_DRAGGED: 
            //printf("tree, dragged\n");
            break;
        default: 
            //printf("tree, %d\n", tree->callback_reason());
            break;
    }
}

/*****************************************************************************/
/* GUI CALLBACK STUFF */
/*****************************************************************************/

void
onGuiFinished(HlabGui *gui_, int argc, char **argv)
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
    
    gui->hexView->setCallback(HexView_cb);

    /* if command line parameter, open that */
    if(argc > 1) {
        file_load(argv[1]); 

        if(argc > 2) {
            load_tags(argv[2]);
        }
    }
    else {
        gui->hexView->setBytes(0, (uint8_t *)initStr, strlen(initStr));

        /* test some colors */
        gui->hexView->hlAdd(3,8,  0xFF0000);
        gui->hexView->hlAdd(8,12, 0x0FF000);
        gui->hexView->hlAdd(12,15,0x00FF00);
        gui->hexView->hlAdd(20,30,0x000FF0);
        gui->hexView->hlAdd(35,40,0x0000FF);
        gui->hexView->hlAdd(50,90,0x880000);
        gui->hexView->hlAdd(100,200,0x088000);
        gui->hexView->hlRanges.print();
    }

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
