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
string assembledBytes;

/*****************************************************************************/
/* FILE HANDLING ROUTINES */
/*****************************************************************************/

/* saves the current buffer to a file
	- path, if given (eg: for Save-As operation)
	- current open file (eg: for Save operation)
*/
int file_save(const char *path)
{
	char *buf = NULL;
	FILE *fp = NULL;
	int len, rc = -1;

	if(!path) {
	    if(!strlen(fileOpenPath)) {
	        printf("ERROR: missing save path\n");
	        goto cleanup;
	    }
	    path = fileOpenPath;
	}

	printf("%s(): saving %s\n", __func__, path);
	fp = fopen(path, "w");
	if(!fp) {
	    printf("ERROR: fopen()\n");
	    goto cleanup;
	}

	buf = gui->srcBuf->text();
	len = strlen(buf);

	printf("%s(): writing 0x%X (%d) bytes\n", __func__, len, len);
	if(len != fwrite(buf, 1, len, fp)) {
	    printf("ERROR: fwrite()\n");
	    goto cleanup;
	}
	
	rc = 0;
	cleanup:
	if(buf) { free(buf); buf = NULL; }
	if(fp) { fclose(fp); fp = NULL; }
	return rc;
}

/* unload current file
	if path is given -> save to that path (save operation)
	if path is not given -> just unload (close operation)
*/
int file_unload(void)
{
	gui->mainWindow->label("Assembler Lab");
	strcpy(fileOpenPath, "");
	gui->srcBuf->text("");

	return 0;
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
	    buf = (unsigned char *)malloc(len+1);
	    if(!buf) {
	        printf("ERROR: malloc()\n");
	        goto cleanup;
	    }
	    buf[len] = '\0';
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

void logNote(const char *msg)
{
	gui->log->setColor(FL_GREEN);
   	gui->log->append("NOTE: ");
	gui->log->setColor(FL_WHITE);
	gui->log->append(msg);
	gui->log->append("\n");
}

void logWarning(const char *msg)
{
	gui->log->setColor(FL_YELLOW);
   	gui->log->append("WARN: ");
	gui->log->setColor(FL_WHITE);
	gui->log->append(msg);
	gui->log->append("\n");
}

void logError(const char *msg)
{
	gui->log->setColor(FL_RED);
   	gui->log->append("ERROR: ");
	gui->log->setColor(FL_WHITE);
	gui->log->append(msg);
	gui->log->append("\n");
}

void assemble_cb(int type, const char *fileName, int lineNum, 
	const char *message)
{
	char buf[128];
	
	sprintf(buf, "line %d: %s", lineNum, message);

	switch(type) {
	    case LLVM_SVCS_CB_NOTE:
	        logNote(buf); break;
	    case LLVM_SVCS_CB_WARNING:
	        logWarning(buf); break;
	    case LLVM_SVCS_CB_ERROR:
		default:
	        logError(buf); break;
	}
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
	
	/* clear the log */
	gui->log->clear(); 
	/* keep the bytes from previous assemble in case this fails */

	/* dialect */
	int dialect = LLVM_SVCS_DIALECT_UNSPEC;
	if(gui->cbAtt->value()) 
	    dialect = LLVM_SVCS_DIALECT_ATT;
	else    
	    dialect = LLVM_SVCS_DIALECT_INTEL;

	/* triplet */
	char triplet[128] = {'\0'};
	strcat(triplet, gui->iArch->value());
	if(gui->iSubArch->size()) {
		strcat(triplet, gui->iSubArch->value());
	}
	if(gui->iVendor->size()) {
		strcat(triplet, "-");
		strcat(triplet, gui->iVendor->value());
	}
	if(gui->iOs->size()) {
		strcat(triplet, "-");
		strcat(triplet, gui->iOs->value());
	}
	if(gui->iEnviron->size()) {
		strcat(triplet, "-");
		strcat(triplet, gui->iEnviron->value());
	}
	
	logNote(triplet);

	/* abi parameter */
	string abi = "none";
	if(strstr(triplet, "mips")) {
		logNote("using MCTargetOptions.abi \"eabi\" for mips");
		abi = "eabi";
	}
		
	/* start */	
	string assembledText, assembledErr;

	vector<int> instrLengths;

	if(0 != llvm_svcs_assemble(srcText, dialect, triplet, abi, 
		LLVM_SVCS_CM_DEFAULT, LLVM_SVCS_RM_DEFAULT, assemble_cb,
		assembledBytes, instrLengths, assembledErr)) {
	    logError("llvm_svcs_assemble()\n");
	    return;
	}

	/* output */
	gui->hexView->clearBytes();
	gui->hexView->setBytes(0, (uint8_t *)(assembledBytes.c_str()), assembledBytes.size());	

	int offs = 0;
	for(auto i=instrLengths.begin(); i!=instrLengths.end(); ++i) {
		//printf("hlAdd( [%d,%d) (%d bytes) )\n", offs, offs+*i, *i);
		gui->hexView->hlAdd(offs, offs+*i);
		offs += *i;
	}

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
}

void new_cb(Fl_Widget *, void *) 
{
	/* unload any current file, clear buffer */
	file_unload();

	/* we "new" by saving the empty text area into a file */
	Fl_File_Chooser chooser(".", "Assembler Files (*.{s,asm})", Fl_File_Chooser::CREATE, "New");
	chooser.show();
	while(chooser.shown()) Fl::wait();
	if(chooser.value() == NULL) return;

	file_save(chooser.value());

}

void save_cb(Fl_Widget *, void *) 
{
	/* file currently open? just write it */
	if(strlen(fileOpenPath)) {
	    file_save(fileOpenPath);
	}
	/* file is not currently open, user selects a new one */
	else {
	    Fl_File_Chooser chooser(".", "Assembler Files (*.{s,asm})", Fl_File_Chooser::CREATE, "Save");
	    chooser.show();
	    while(chooser.shown()) Fl::wait();
	    if(chooser.value() == NULL) return;

	    file_save(chooser.value());

	    /* saved file becomes currently opened file (unlike "Save-As") */
	    strcpy(fileOpenPath, chooser.value());
	}
}

void saveas_cb(Fl_Widget *, void *) 
{
	Fl_File_Chooser chooser(".", "Assembler Files (*.{s,asm})", Fl_File_Chooser::CREATE, "Save As");
	chooser.show();
	while(chooser.shown()) Fl::wait();
	if(chooser.value() == NULL) return;

	file_save(chooser.value());
	/* saved file does NOT become currently opened file */
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
	assemble_requested = true;
}

void
onExampleSelection()
{
	const char *eg = gui->icExamples->value();
	const char *triplet;

	if(0==strcmp(eg, "i386 (at&t)")) {
	    gui->srcBuf->text((char *)rsrc_x86_s);
	    gui->cbAtt->value(1);
		triplet = "i386-none-none";
	}
	else if(0==strcmp(eg, "i386 (intel)")) {
	    gui->srcBuf->text((char *)rsrc_x86_intel_s);
	    gui->cbAtt->value(0);
		triplet = "i386-none-none";
	}
	else if(0==strcmp(eg, "x86_64 (at&t)")) {
	    gui->srcBuf->text((char *)rsrc_x86_64_s);
	    gui->cbAtt->value(1);
		triplet = "x86_64-none-none";
	}
	else if(0==strcmp(eg, "x86_64 (intel)")) {
	    gui->srcBuf->text((char *)rsrc_x86_64_intel_s);
	    gui->cbAtt->value(0);
		triplet = "x86_64-none-none";
	}
	else if(0==strcmp(eg, "powerpc")) {
	    gui->srcBuf->text((char *)rsrc_ppc_s);
		triplet = "powerpc-none-none";
	}
	else if(0==strcmp(eg, "powerpc64")) {
	    gui->srcBuf->text((char *)rsrc_ppc64_s);
		triplet = "powerpc64-none-none";
	}
	else if(0==strcmp(eg, "powerpc64le")) {
	    gui->srcBuf->text((char *)rsrc_ppc64_s);
		triplet = "powerpc64le-none-none";
	}
	else if(0==strcmp(eg, "arm")) {
	    gui->srcBuf->text((char *)rsrc_arm_s);
		triplet = "arm-none-none-eabi";
	}
	else if(0==strcmp(eg, "thumb")) {
	    gui->srcBuf->text((char *)rsrc_thumb_s);
		triplet = "thumb-none-none";
	}
	else if(0==strcmp(eg, "aarch64")) {
	    gui->srcBuf->text((char *)rsrc_arm64_s);
		triplet = "aarch64-none-none-eabi";
	}
	else if(0==strcmp(eg, "mips")) {
	    gui->srcBuf->text((char *)rsrc_mips_s);
		triplet = "mips-pc-eabi";
	}

	string arch, subarch, vendor, os, environ, objFormat;
	llvm_svcs_triplet_decompose(triplet, arch, subarch, vendor, os, environ,
	    objFormat);
	gui->iArch->value(arch.c_str());
	gui->iSubArch->value(subarch.c_str());
	gui->iVendor->value(vendor.c_str());
	gui->iOs->value(os.c_str());
	gui->iEnviron->value(environ.c_str());
	gui->oFormat->value(objFormat.c_str());

	assemble_forced = true;
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
	    { "&File",            0, 0, 0, FL_SUBMENU },
	    { "&New",        0, (Fl_Callback *)new_cb },
	    { "&Open",    FL_COMMAND + 'o', (Fl_Callback *)open_cb, 0, FL_MENU_DIVIDER },
//        { "&Insert File...",  FL_COMMAND + 'i', (Fl_Callback *)insert_cb, 0, FL_MENU_DIVIDER },
	    { "&Save",       FL_COMMAND + 's', (Fl_Callback *)save_cb },
	    { "Save &As...", FL_COMMAND + FL_SHIFT + 's', (Fl_Callback *)saveas_cb, 0, FL_MENU_DIVIDER },
	    { "&Close",           FL_COMMAND + 'w', (Fl_Callback *)close_cb, 0, FL_MENU_DIVIDER },
	    { "E&xit",            FL_COMMAND + 'q', (Fl_Callback *)quit_cb, 0 },
	    { 0 }
	};
	
	gui->menuBar->copy(menuItems);

	// can test triples with:
	// clang -S -c -target ppc32le-apple-darwin hello.c
	// see also lib/Support/Triple.cpp in llvm source
	// see also http://clang.llvm.org/docs/CrossCompilation.html
	// TODO: add the default machine config */
	const char *examples[11] = { "i386 (at&t)", "i386 (intel)", "x86_64 (at&t)",
		"x86_64 (intel)", "arm", "thumb", "aarch64", "powerpc", "powerpc64",
		"powerpc64le", "mips" };
	for(int i=0; i<11; ++i) {
		gui->icExamples->add(examples[i]);
	}
	gui->icExamples->value(0);
	onExampleSelection();

	rc = 0;
	//cleanup:
	return;
}

void
onBtnTest()
{
	printf("clicked test!\n");
	gui->hexView->clearBytes();
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
