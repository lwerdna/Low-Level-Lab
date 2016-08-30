/* c stdlib includes */
#include <time.h>
#include <stdio.h>

/* c++ includes */
#include <map>
#include <string>
#include <vector>
using namespace std;

#include "rsrc.h"
#include "AlabGui.h"
#include "llvm_svcs.h"

/* fltk includes */
#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_File_Chooser.H>

/* autils */
extern "C" {
#include <autils/crc.h>
#include <autils/output.h>
#include <autils/filesys.h>
#include <autils/subprocess.h>
}

/* globals */
AlabGui *gui = NULL;
char fileOpenPath[FILENAME_MAX] = {'\0'};

/* reassemble state vars */
bool assemble_forced = false;
bool assemble_requested = false;
int length_last_assemble = 0;
time_t time_last_assemble = 0;
uint32_t crc_last_assemble = 0;
/* configuration string */
const char *configTriple = NULL;

int file_unload(void)
{
    int rc = -1;

    gui->mainWindow->label("Assembler Lab");
    strcpy(fileOpenPath, "");
    gui->srcBuf->text("");
    rc = 0;

    return rc;
}

int file_load(const char *path) 
{
    int rc = -1;
    unsigned char *buf = NULL;
    size_t len;

    FILE *fp = fopen(path, "rb");
    if(!fp) {
        printf("ERROR: fopen()\n");
        goto cleanup;
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(len == 0) {
        gui->srcBuf->text("");
    }
    else {
        buf = (unsigned char *)malloc(len);
        if(!buf) {
            printf("ERROR: malloc()\n");
            goto cleanup;
        }
    }

    if(len != fread(buf, 1, len, fp)) {
        printf("ERROR: fread()\n");
        goto cleanup;
    }

    gui->mainWindow->label(path);
    gui->srcBuf->text((char *)buf);
    strcpy(fileOpenPath, path);
    rc = 0;

    cleanup:

    if(rc) {
        if(buf) {
            free(buf);
            buf = NULL;
        }
    }

    return rc;
}

/*****************************************************************************/
/* MAIN ROUTINE */
/*****************************************************************************/

void assemble_cb(int type, const char *fileName, int lineNum, 
    const char *message)
{
    char buf[32];

    switch(type) {
        case LLVM_SVCS_CB_NOTE:
            gui->log->setColor(FL_GREEN);
            gui->log->append("NOTE ");
            break;
        case LLVM_SVCS_CB_WARNING:
            gui->log->setColor(FL_YELLOW);
            gui->log->append("WARN ");
            break;
        case LLVM_SVCS_CB_ERROR:
            gui->log->setColor(FL_RED);
            gui->log->append("ERROR ");
            break;
    }

    gui->log->setColor(FL_WHITE);
    sprintf(buf, "line %d: ", lineNum);
    gui->log->append(buf);
    gui->log->append(message);
    sprintf(buf, "\n");
    gui->log->append(buf);
}

void
assemble()
{
    int rc = -1;
    char *srcText = NULL;

    /* assemble timing vars */
    time_t time_now = 0;
    int length_now = 0;
    uint32_t crc_now = 0;

    Fl_Text_Buffer *srcBuf = gui->srcBuf;
    Fl_Text_Buffer *bytesBuf = gui->bytesBuf;

    srcText = srcBuf->text();

    if(!assemble_forced && !assemble_requested) {
        //printf("skipping assemble, neither forced nor requested\n");
        return;
    }
    
    if(!assemble_forced && assemble_requested) {
        /* skip if we assembled within the last second */
        time(&time_now);
        float delta = difftime(time_now, time_last_assemble);
        if(delta >= 0 && delta < 1) {
            /* just too soon, remain requested */
            //printf("skipping assemble, too soon (now,last,diff)=(%ld,%ld,%f)\n",
            //    time_now, time_last_assemble, difftime(time_now, time_last_assemble));
            return;
        }
        else {
            /* skip if the text is unchanged */
            length_now = srcBuf->length();
            if(length_last_assemble == length_now) {
                crc_now = crc32(0, srcText, srcBuf->length());
                if(crc_now == crc_last_assemble) {
                    //printf("skipping assemble, buffer unchanged\n");
                    assemble_requested = false;
                    return;
                }
            }
        }
    }

    assemble_forced = false;
    assemble_requested = false;
    if(crc_now) crc_last_assemble = crc_now;
    if(time_now) time_last_assemble = time_now;
    if(length_now) length_last_assemble = length_now;
    
    /* clear the output text */
    bytesBuf->text("");
    /* clear the log output */
    gui->log->clear(); 

    int dialect = LLVM_SVCS_DIALECT_UNSPEC;
    if(gui->cbAtt->value()) 
        dialect = LLVM_SVCS_DIALECT_ATT;
    else    
        dialect = LLVM_SVCS_DIALECT_INTEL;
    string assembledText, assembledBytes, assembledErr;
    if(0 != llvm_svcs_assemble(srcText, dialect, configTriple, assembledText, 
      assembledBytes, assembledErr, assemble_cb)) {
        printf("ERROR: llvm_svcs_assemble()\n");
        printf("%s\n", assembledErr.c_str());
        return;
    }

    /* output */
    bytesBuf->text(assembledText.c_str());

    rc = 0;
    //cleanup:
    if(srcText) {
        free(srcText);
        srcText = NULL;
    }
    return;
}

/*****************************************************************************/
/* MENU CALLBACKS */
/*****************************************************************************/

void open_cb(Fl_Widget *, void *)
{
    Fl_File_Chooser chooser(".", "Assembler Files (*.{s,asm})", Fl_File_Chooser::SINGLE, "Open");
    chooser.show();
    while(chooser.shown()) Fl::wait();
    if(chooser.value() == NULL) return;
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
   file_unload();
   return;
}

void quit_cb(Fl_Widget *, void *) {
    file_unload();
    gui->mainWindow->hide();
}

/*****************************************************************************/
/* GUI CALLBACK STUFF */
/*****************************************************************************/

/* reassemble request from GUI with 0 args */
void
reassemble(void)
{
    printf("reassemble request\n");
    assemble_forced = true;
}

/* callback when the source code is changed
    (normal callback won't trigger during text change) */
void
onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg)
{
    printf("%s()\n", __func__);
    assemble_requested = true;
}

void
onConfigSelection()
{
    configTriple = gui->icPresets->value();
    printf("you selected: %s\n", configTriple);

    string arch, vendor, os, environ, objFormat;
    llvm_svcs_triplet_decompose(configTriple, arch, vendor, os, environ,
        objFormat);

    gui->oArch->value(arch.c_str());
    gui->oVendor->value(vendor.c_str());
    gui->oOs->value(os.c_str());
    gui->oEnviron->value(environ.c_str());
    gui->oFormat->value(objFormat.c_str());

    /* with new choice, reassemble */
    assemble_forced = true;
}

void
onExampleSelection()
{
    const char *file = gui->icExamples->value();

    if(0==strcmp(file, "x86.s")) {
        gui->srcBuf->text((char *)rsrc_x86_s);
        gui->cbAtt->value(1);
    }
    if(0==strcmp(file, "x86_intel.s")) {
        gui->srcBuf->text((char *)rsrc_x86_intel_s);
        gui->cbAtt->value(0);
    }
    else if(0==strcmp(file, "x86_64.s")) {
        gui->cbAtt->value(1);
        gui->srcBuf->text((char *)rsrc_x86_64_s);
    }
    else if(0==strcmp(file, "x86_64_intel.s")) {
        gui->cbAtt->value(0);
        gui->srcBuf->text((char *)rsrc_x86_64_intel_s);
    }
    else if(0==strcmp(file, "ppc32.s")) {
        gui->srcBuf->text((char *)rsrc_ppc_s);
    }
    else if(0==strcmp(file, "ppc64.s")) {
        gui->srcBuf->text((char *)rsrc_ppc64_s);
    }
    else if(0==strcmp(file, "arm.s")) {
        gui->srcBuf->text((char *)rsrc_arm_s);
    }
    else if(0==strcmp(file, "thumb.s")) {
        gui->srcBuf->text((char *)rsrc_thumb_s);
    }
    else if(0==strcmp(file, "arm64.s")) {
        gui->srcBuf->text((char *)rsrc_arm64_s);
    }
}

void
onDialectChange()
{
    int state = gui->cbAtt->value();
    printf("new at&t check state: %d\n", state);
    assemble_forced = true;
}

void
onGuiFinished(AlabGui *gui_)
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
        { 0 }
    };
    
    gui->menuBar->copy(menuItems);

    /* initial input choices */
    gui->icExamples->add("x86_intel.s");
    gui->icExamples->add("x86.s");
    gui->icExamples->add("x86_64_intel.s");
    gui->icExamples->add("x86_64.s");
    gui->icExamples->add("ppc32.s");
    gui->icExamples->add("ppc64.s");
    gui->icExamples->add("arm.s");
    gui->icExamples->add("thumb.s");
    gui->icExamples->add("arm64.s");
    gui->icExamples->value(0);
    gui->cbAtt->value(0);
    onExampleSelection();

    // can test triples with:
    // clang -S -c -target ppc32le-apple-darwin hello.c
    // see also lib/Support/Triple.cpp in llvm source
    // see also http://clang.llvm.org/docs/CrossCompilation.html
    // TODO: add the default machine config */
    gui->icPresets->add("i386-none-none");
    gui->icPresets->add("x86_64-none-none");
    gui->icPresets->add("arm-none-none-eabi");
    gui->icPresets->add("thumb-none-none");
    gui->icPresets->add("aarch64-none-none-eabi");
    gui->icPresets->add("powerpc-none-none");
    gui->icPresets->add("powerpc64-none-none");
    gui->icPresets->add("powerpc64le-none-none");
    gui->icPresets->add("haha-just-a-joke");
    /* start it at the 0'th value */
    gui->icPresets->value(0);
    /* pretend the user did it */
    onConfigSelection();

    assemble_forced = true;

    rc = 0;
    //cleanup:
    return;
}

void
onIdle(void *data)
{
    assemble();
}

void
onExit(void)
{
    printf("onExit()\n");
}
