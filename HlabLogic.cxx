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
#include "tagging.h"

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
	#include <autils/filesys.h>
}

/* forward dec's */
void tree_cb(Fl_Tree *, void *);
int tags_load_file(const char *target);

/* globals */
HlabGui *gui = NULL;

FILE *fileOpenFp = NULL;
void *fileOpenPtrMap = NULL;
size_t fileOpenSize = 0;

IntervalMgr intervMgr; /* global to hold all intervals */
map<Fl_Tree_Item *, Interval *> treeItemToInterv;
Fl_Window *winTags = NULL;
Fl_Tree *tree = NULL;

const char *initStr = "This_is_the_default_bytes_when_no_file_is_open._Here's_some_deadbeef:_\xDE\xAD\xBE\xEF";

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
	
	gui->mainWindow->label(path);

	tags_load_file(path);
		
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

void tags_fill_tree_dfs(Fl_Tree *tree, Fl_Tree_Item *item, Interval *tag)
{
	/* insert current guy */
	const char *tagName = tag->data_string.c_str();
	Fl_Tree_Item *itemNew = tree->add(item, tagName);
	itemNew->close();

	/* save the Tree_Item -> Interval Mapping */
	treeItemToInterv[itemNew] = tag;

	/* insert all his children */
	vector<Interval *> children = tag->childRetrieve();
	for(auto iter = children.begin(); iter != children.end(); iter++) {
		Interval *childTag = (*iter);
		tags_fill_tree_dfs(tree, itemNew, childTag);
	}
}

int tags_load_file(const char *target)
{
	int rc = -1;

	vector<string> taggers;
	vector<Interval *> tags;
	Fl_Tree_Item *rootItem;

	if(0 != tagging_pollall(target, taggers)) goto cleanup;
	// TODO: popup and let user decide if there are >1 taggers
	if(taggers.size()) {
		printf("going with tagger: %s\n", taggers[0].c_str());
	}
	else {
		printf("no tagger recognized the file\n");
		goto cleanup;
	}

	if(0 != tagging_tag(target, taggers[0], /* out */ intervMgr)) {
		printf("ERROR: tagging_tag()\n");
		goto cleanup;
	}

	/* add new tree item to the tree window */
	if(0 == intervMgr.size()) {
		printf("ERROR: no data received from tagger\n");
		goto cleanup;
	}

	winTags = new Fl_Window(
		gui->mainWindow->x()+gui->mainWindow->w()+32, 
		gui->mainWindow->y(), gui->mainWindow->w(), 
		gui->mainWindow->h(), 
		"tags"
	);
	tree = new Fl_Tree(0, 0, winTags->w(), winTags->h());
	tree->end();
	tree->showroot(0);
	tree->callback((Fl_Callback *)tree_cb);
	winTags->end();
	winTags->resizable(tree);

	//tree->root_label("file");
	rootItem = tree->root();

	tags = intervMgr.findParentChild();
	for(int i=0; i<tags.size(); ++i) {
		tags_fill_tree_dfs(tree, rootItem, tags[i]);
	}

	/* show window */
	winTags->show();

	rc = 0;
	cleanup:
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
	Fl_File_Chooser chooser(
		".",	// directory
		"*",	// filter
		Fl_File_Chooser::MULTI, // type
		"Edit File"			 // title
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
	//printf("	VALUE: '%s'\n", chooser.value());
	//printf("	COUNT: %d files selected\n", chooser.count());

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
	if(winTags) { winTags->hide(); }
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
		".",	// directory
		"*",	// filter
		Fl_File_Chooser::MULTI, // type
		"Edit File"			 // title
	);

	chooser.show();

	while(chooser.shown()) {
		Fl::wait();
	}

	if(chooser.value() == NULL) {
		return;
	}

	tags_load_file(chooser.value());

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
				printf("tree item not found in item->ival map!\n");
			}
			else { 
				Interval *ival = treeItemToInterv[item];
				uint32_t palette[5] = {0xff00ff, 0xbf00bf, 0x7f007f, 0x3f003f, 0x000000};
				//ival->print();

				/* move upwards along the tree, coloring */
				//uint32_t palette[5] = {0xece2a5, 0xa5cebc, 0x768aa5, 0x496376, 0xa9212e};
				//uint32_t palette[5] = {0xFFFF00, 0x0000FF, 0x088000, 0x880000, 0xFF0000};

				vector<Fl_Tree_Item *> lineage;
 
				Fl_Tree_Item *curr = item;
				while(curr) {
					lineage.insert(lineage.end(), curr);
					curr = curr->parent();
				}
				
				gui->hexView->hlClear();
				for(int i=0; i<lineage.size(); ++i) {
					if(treeItemToInterv.find(lineage[i]) == treeItemToInterv.end()) {
						// end of tree climb?
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
		{ "&File",			  0, 0, 0, FL_SUBMENU },
//		{ "&New File",		0, (Fl_Callback *)new_cb },
		{ "&Open",	FL_COMMAND + 'o', (Fl_Callback *)open_cb },
//		{ "&Insert File...",  FL_COMMAND + 'i', (Fl_Callback *)insert_cb, 0, FL_MENU_DIVIDER },
//		{ "&Save File",	   FL_COMMAND + 's', (Fl_Callback *)save_cb },
//		{ "Save File &As...", FL_COMMAND + FL_SHIFT + 's', (Fl_Callback *)saveas_cb, 0, FL_MENU_DIVIDER },
		{ "&Close",		   FL_COMMAND + 'w', (Fl_Callback *)close_cb, 0, FL_MENU_DIVIDER },
		{ "E&xit",			FL_COMMAND + 'q', (Fl_Callback *)quit_cb, 0 },
		{ 0 },

		{ "&Tags", FL_COMMAND, (Fl_Callback *)tags_cb },
		{ 0 },

//		{ "&Edit", 0, 0, 0, FL_SUBMENU },
//		{ "Cu&t",			 FL_COMMAND + 'x', (Fl_Callback *)cut_cb },
//		{ "&Copy",			FL_COMMAND + 'c', (Fl_Callback *)copy_cb },
//		{ "&Paste",		   FL_COMMAND + 'v', (Fl_Callback *)paste_cb },
//		{ "&Delete",		  0, (Fl_Callback *)delete_cb },
//		{ 0 },

//		{ "&Search", 0, 0, 0, FL_SUBMENU },
//		{ "&Find...",		 FL_COMMAND + 'f', (Fl_Callback *)find_cb },
//		{ "F&ind Again",	  FL_COMMAND + 'g', find2_cb },
//		{ "&Replace...",	  FL_COMMAND + 'r', replace_cb },
//		{ "Re&place Again",   FL_COMMAND + 't', replace2_cb },
//		{ 0 },

		{ 0 }
	};
	
	gui->menuBar->copy(menuItems);
	
	gui->hexView->setCallback(HexView_cb);

	/* if command line parameter, open that */
	if(argc > 1) {
		file_load(argv[1]); 

		if(argc > 2) {
			tags_load_file(argv[2]);
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
		//gui->hexView->hlRanges.print();
	}

	rc = 0;
	//cleanup:
	return;
}

void
onIdle(void *data)
{
	static int i=0;
	printf("idle! %d\n", i++);	
}

void
onExit(void)
{
	printf("onExit()\n");
}
