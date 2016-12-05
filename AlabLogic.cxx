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
int dialect = LLVM_SVCS_DIALECT_UNSPEC;
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
/* LOGGING STUFF */
/*****************************************************************************/

void 
logNote(const char *msg)
{
	gui->log->setColor(FL_GREEN);
   	gui->log->append("NOTE: ");
	gui->log->setColor(FL_WHITE);
	gui->log->append(msg);
	gui->log->append("\n");
}

void 
logWarning(const char *msg)
{
	gui->log->setColor(FL_YELLOW);
   	gui->log->append("WARN: ");
	gui->log->setColor(FL_WHITE);
	gui->log->append(msg);
	gui->log->append("\n");
}

void 
logError(const char *msg)
{
	gui->log->setColor(FL_RED);
   	gui->log->append("ERROR: ");
	gui->log->setColor(FL_WHITE);
	gui->log->append(msg);
	gui->log->append("\n");
}

/*****************************************************************************/
/* ASSEMBLE */
/*****************************************************************************/

int 
getCodeModel()
{
	const char *guiStr = gui->icCodeModel->value();

	if(!strcmp(guiStr, "jit"))
		return LLVM_SVCS_CM_DEFAULT;
	if(!strcmp(guiStr, "small"))
		return LLVM_SVCS_CM_SMALL;
	if(!strcmp(guiStr, "kernel"))
		return LLVM_SVCS_CM_KERNEL;
	if(!strcmp(guiStr, "medium"))
		return LLVM_SVCS_CM_MEDIUM;
	if(!strcmp(guiStr, "large"))
		return LLVM_SVCS_CM_LARGE;

	logError("invalid code model selection, defaulting to JIT");
	return LLVM_SVCS_CM_DEFAULT;
}

int 
getRelocModel()
{
	const char *guiStr = gui->icRelocModel->value();

	if(!strcmp(guiStr, "static"))
		return LLVM_SVCS_RM_STATIC;
	if(!strcmp(guiStr, "pic"))
		return LLVM_SVCS_RM_PIC;
	if(!strcmp(guiStr, "dynamic"))
		return LLVM_SVCS_RM_DYNAMIC_NO_PIC;

	logError("invalid reloc model selection, defaulting to static");
	return LLVM_SVCS_RM_STATIC;
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
assemble(void *)
{
	int rc = -1, offs;
	char *src_text = NULL;

	/* clear the log */
	gui->log->clear(); 
	logNote("assembling");

	/* do NOT clear the log
		keep the bytes from previous assemble in case this fails */

	/* reassemble the triplet from the GUI components
	  (to take into immediate account user selections) */
	char triplet[128] = {'\0'};
	strcat(triplet, gui->iArch->value());

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
	//if(strstr(triplet, "mips")) {
	//	logNote("using MCTargetOptions.abi \"eabi\" for mips");
	//	abi = "o32";
	//}
	
	/* start */
	string assembledText, assembledErr;
	src_text = gui->srcBuf->text();
	vector<int> instrLengths;

	if(llvm_svcs_assemble(src_text, dialect, triplet, 
	  getCodeModel(), getRelocModel(), assemble_cb,
	  assembledBytes, assembledErr)) {
	    logError("llvm_svcs_assemble()");
		logError(assembledErr.c_str());
	    goto cleanup;
	}

	/* output */
	gui->hexView->clearBytes();
	gui->hexView->setBytes(0, (uint8_t *)(assembledBytes.c_str()), assembledBytes.size());

	if(llvm_svcs_disasm_lengths(triplet, (uint8_t *)assembledBytes.c_str(), assembledBytes.size(),
	  0, instrLengths)) {
		logError("llvm_svcs_disasm_lengths()");
		goto cleanup;
	}

	offs = 0;
	for(auto it=instrLengths.begin(); it!=instrLengths.end(); ++it) {
		//printf("hlAdd( [%d,%d) (%d bytes) )\n", offs, offs+*it, *it);
		gui->hexView->hlAdd(offs, offs+*it);
		offs += *it;
	}

	/* done */
	rc = 0;
	cleanup:
	if(src_text) free(src_text);
}

/* manages assemble requests
	- don't assemble too frequently (wait .5 seconds after last modifcation)
	- don't assemble if unnecessary (if source buf not changed)
*/
void
assemble_schedule(bool forced)
{
	int rc = -1;
	uint32_t crc = 0;
	static uint32_t crc_last = 0;

	Fl_Text_Buffer *srcBuf = gui->srcBuf;
	char *src_text = srcBuf->text();

	/* if source not modified, skip */
	if(!forced) {
		crc = crc32(0, src_text, srcBuf->length());
		if(crc == crc_last) {
			goto cleanup;
		}
	}

	crc_last = crc;

	/* remove previously scheduled call
		(additional keystrokes schedule a NEW callback) */
	Fl::remove_timeout(assemble);
	Fl::add_timeout(.5, assemble);

	cleanup:
	if(src_text) free(src_text);
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

/* callback when the source code is changed
	(normal callback won't trigger during text change) */
void
onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
	const char * deletedText, void *cbArg)
{
	assemble_schedule(false);
}

void
setTripletAndReassemble(const char *triplet, char *src, int src_len, 
  int dialect_)
{
	/* set dialect (UNSPECIFIED, INTEL, ATT) */
	dialect = dialect_;

	/* set the arch, vendor, os, environment */
	string arch, subarch, vendor, os, environ, objFormat;
	llvm_svcs_triplet_decompose(triplet, arch, subarch, vendor, os, environ,
	    objFormat);

	gui->iArch->value(arch.c_str());
	gui->iArch->do_callback();
	gui->iVendor->value(vendor.c_str());
	gui->iVendor->do_callback();
	gui->iOs->value(os.c_str());
	gui->iOs->do_callback();
	gui->iEnviron->value(environ.c_str());
	gui->iEnviron->do_callback();

	/* set the source code buffer
		this also triggers the modify callback (which schedules assemble) */
	string tmp(src, src_len);
	gui->srcBuf->text(tmp.c_str());
}

void onBtnX86()
{
	setTripletAndReassemble("i386-none-none", (char *)rsrc_x86_s,
		sizeof(rsrc_x86_s), LLVM_SVCS_DIALECT_ATT);
}

void onBtnX86_()
{
	setTripletAndReassemble("i386-none-none", (char *)rsrc_x86_intel_s, 
		sizeof(rsrc_x86_intel_s),LLVM_SVCS_DIALECT_INTEL);
}

void onBtnX64()
{
	setTripletAndReassemble("x86_64-none-none", (char *)rsrc_x86_64_s, 
		sizeof(rsrc_x86_64_s),LLVM_SVCS_DIALECT_ATT);
}

void onBtnX64_()
{
	setTripletAndReassemble("x86_64-none-none", (char *)rsrc_x86_64_intel_s, 
		sizeof(rsrc_x86_64_intel_s),LLVM_SVCS_DIALECT_INTEL);
}

void onBtnMips()
{
	setTripletAndReassemble("mips-pc-none-o32", (char *)rsrc_mips_s, 
		sizeof(rsrc_mips_s),0);
}

void onBtnArm()
{
	setTripletAndReassemble("armv7-none-none", (char *)rsrc_arm_s, 
		sizeof(rsrc_arm_s),0);
}

void onBtnArm64()
{
	setTripletAndReassemble("aarch64-none-none", (char *)rsrc_arm64_s, 
		sizeof(rsrc_arm64_s),0);
}

void onBtnPpc()
{
	setTripletAndReassemble("powerpc-none-none", (char *)rsrc_ppc_s, 
		sizeof(rsrc_ppc_s),0);
}

void onBtnPpc64()
{
	setTripletAndReassemble("powerpc64-none-none", (char *)rsrc_ppc64_s, 
		sizeof(rsrc_ppc64_s),0);
}

void onBtnPpc64le()
{
	setTripletAndReassemble("powerpc64le-none-none", (char *)rsrc_ppc64_s, 
		sizeof(rsrc_ppc64_s),0);
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

	// LLVM STUFF
	llvm_svcs_init();

	/* init code models */
	const char *cms[5] = { "jit", "small", "kernel", "medium", "large" };
	for(int i=0; i<5; ++i)
		gui->icCodeModel->add(cms[i]);
	gui->icCodeModel->value(0);

	/* init reloc models */
	const char *rms[3] = { "static", "pic", "dynamic" };
	for(int i=0; i<3; ++i)
		gui->icRelocModel->add(rms[i]);
	gui->icRelocModel->value(0);

	/* start by pretending the user hit x86 (intel syntax) */
	onBtnX86_();

	rc = 0;
	//cleanup:
	return;
}

void
onIcCodeModel()
{
	//printf("%s()\n", __func__);
	assemble_schedule(true);
}

void
onIcRelocModel()
{
	//printf("%s()\n", __func__);
	assemble_schedule(true);
}

void
onInpArch()
{
	printf("%s()\n", __func__);
	assemble_schedule(true);
}

void
onInpVendor()
{
	//printf("%s()\n", __func__);
	assemble_schedule(true);
}

void
onInpOs()
{
	//printf("%s()\n", __func__);
	assemble_schedule(true);
}

void
onInpEnviron()
{
	//printf("%s()\n", __func__);
	assemble_schedule(true);
}

void
onExit(void)
{
	printf("onExit()\n");
}
