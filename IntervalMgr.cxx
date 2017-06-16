#include <stdint.h>
#include <stdlib.h>

/* c++ */
#include <vector>
#include <algorithm>
using namespace std;

/* google regex */
#include <re2/re2.h>

/* autils */
extern "C" {
	#include <autils/bytes.h>
}

/* fltk */
#include <FL/Fl.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Double_Window.H>

/* local */
#include "IntervalMgr.h"

//#define INTERVAL_MGR_DEBUG 1

/*****************************************************************************/
/* compare functions */
/*****************************************************************************/

bool compareByStartAddr(Interval a, Interval b)
{
	return a.left < b.left;
}

bool compareByLength(Interval a, Interval b)
{
	return a.length > b.length;
}

bool compareByStartAddrP(Interval *a, Interval *b)
{
	return a->left < b->left;
}

bool compareByLengthP(Interval *a, Interval *b)
{
	return a->length > b->length;
}

/*****************************************************************************/
/* interval class */
/*****************************************************************************/

Interval::Interval(uint64_t left_, int right_)
{
	left = left_;
	right = right_; // [,)
	length = right - left;
}

Interval::Interval(uint64_t left_, int right_, void *data_void_ptr_)
{
	left = left_;
	right = right_; // [,)
	length = right - left;
	data_type = 1;
	data_void_ptr = data_void_ptr_;
}

Interval::Interval(uint64_t left_, int right_, uint32_t data_u32_)
{
	left = left_;
	right = right_; // [,)
	length = right - left;
	data_type = 2;
	data_u32 = data_u32_;
}

Interval::Interval(uint64_t left_, int right_, string data_string_)
{
	left = left_;
	right = right_; // [,)
	length = right - left;
	data_type = 3;
	data_string = data_string_;
}

Interval::~Interval()
{
}

bool Interval::contains(uint64_t addr)
{
	return (addr >= left && addr < right);
}

bool Interval::contains(Interval &ival)
{
	return contains(ival.left) && contains(ival.right-1);
}

bool Interval::intersects(Interval &ival)
{
	return contains(ival.left) || contains(ival.right-1);
}
	
void Interval::childAdd(Interval *child)
{
	children.push_back(child);
}

void Interval::childSortByAddr(void)
{
	std::sort(children.begin(), children.end(), compareByStartAddrP); 

	for(auto i=children.begin(); i!=children.end(); ++i) {
		(*i)->childSortByAddr();
	}
}

void Interval::childSortByLength(void)
{
	std::sort(children.begin(), children.end(), compareByLengthP); 

	for(auto i=children.begin(); i!=children.end(); ++i) {
		(*i)->childSortByLength();
	}
}

vector<Interval *> Interval::childRetrieve()
{
	return children;
}

bool Interval::childCheck(Interval *orphan)
{
	for(auto i=children.begin(); i!=children.end(); ++i) {
		if(*i == orphan)
			return true;
	}

	return false;
}

void Interval::print(bool recur=false, int depth=0)
{
	for(int i=0; i<depth; ++i)
		printf("  ");
	
	printf("interval@%p [%016llX,%016llX)", this, left, right);
	if(data_type) printf(" data: ");
	switch(data_type) {
		case 1: printf("%p", data_void_ptr); break;
		case 2: printf("0x%08X", data_u32); break;
		case 3: printf("\"%s\"", data_string.c_str()); break;
	}

	if(recur && children.size()) {
		printf("\n");
		for(auto iter = children.begin(); iter != children.end(); iter++) {
			(*iter)->print(true, depth+1);
		}
	}
	else if(children.size() > 0)
		printf(" (%ld children)\n", children.size());
	else
		printf("\n");

}

/*****************************************************************************/
/* interval manager class */
/*****************************************************************************/

IntervalMgr::~IntervalMgr()
{
	/* c++ noob explicitly calls vector clear() which I hope will call all
		destructors of members */
	intervals.clear(); 
}

void IntervalMgr::add(Interval iv)
{
	#ifdef INTERVAL_MGR_DEBUG
	printf("add(): ");
	iv.print();
	#endif
	intervals.push_back(iv);
}
	
unsigned int IntervalMgr::size(void)
{
	return intervals.size();
}

void IntervalMgr::clear()
{
	searchPrepared = false;
	intervals.clear();
}

int IntervalMgr::readFromFilePointer(FILE *fp)
{
	int rc = -1;
	char *line = NULL;
	int len;

	for(int line_num=1; 1; ++line_num) {
		uint64_t start, end;
		uint32_t color;
		size_t line_allocd = 0;

		if(line) {
			free(line);
			line = NULL;
			line_allocd = 0;
		}
		if(getline(&line, &line_allocd, fp) <= 0) {
			break; // don't whine, either error or EOF
		}

		/* if whitespace */
		if(RE2::FullMatch(line, "\\s*"))
			continue;

		/* if comment */
		if(RE2::FullMatch(line, "\\s*//.*"))
			continue;

		/* if interval */
		string regex;
		regex += "\\s*\\[\\s*";
		regex += "(?:0x)?([[:xdigit:]]{1,16})"; /* start address */
		regex += "\\s*,\\s*";
		regex += "(?:0x)?([[:xdigit:]]{1,16})"; /* end address */
		regex += "\\s*\\)\\s+";
		regex += "(?:0x)?([[:xdigit:]]{1,8})"; /* color */
		regex += "\\s+";
		regex += "(.*)\n?";						/* comment */

		string a, b, c, d;
		if(!RE2::FullMatch(line, regex.c_str(), &a, &b, &c, &d)) {
			printf("ERROR: malformed input on line %d: -%s-\n", line_num, line);
			goto cleanup;
		}

		parse_uint64_hex(a.c_str(), &start);
		parse_uint64_hex(b.c_str(), &end);
		parse_uint32_hex(c.c_str(), &color);

		/* done, add interval */
		Interval ival = Interval(start, end, d);
		add(ival);
	}

	rc = 0;

	cleanup:
	/* this free must occur even if getline() failed */
	if(line) free(line);

	return rc;
}

int IntervalMgr::readFromFile(char *fpath)
{
	int rc = -1;
	FILE *fp = NULL;

	fp = fopen(fpath, "r");
	if(!fp) {
		printf("ERROR: fopen()\n");
		goto cleanup;
	}

	rc = readFromFilePointer(fp); 

	cleanup:
	if(fp) fclose(fp);
	return rc;	
}

/* sort by interval start address */
void IntervalMgr::sortByStartAddr()
{
	std::sort(intervals.begin(), intervals.end(), compareByStartAddr); 
}

/* sort by interval lengths */
void IntervalMgr::sortByLength()
{
	std::sort(intervals.begin(), intervals.end(), compareByLength); 
}

/* GOAL: log_2(n) search in set of possibly overlapping intervals, with
	priority going to smaller intervals

	Step1: sort the intervals in decreasing order by size of interval
	Step2: insert the intervals, largest first, into new list ... the smaller
		intervals can split the previous ones upon entry, giving them higher
		priority
	Step3: sort the resulting non-overlapping intervals by starting address

	now you can just binary search
*/

void IntervalMgr::searchFastPrep()
{
	// STEP 1: sorts intervals by descending size, overlap is still possible here
	sortByLength();
   
	// STEP 2: insert into new list the largest intervals first (smaller
	//		 intervals will overwrite and thus have higher priority
	//
	//		 insertion will split pre-existing into non-overlapping intervals
	vector<Interval> flatList;

	// insert into sorted (by start addr) list (removing overlap)
	for(unsigned int i=0; i<intervals.size(); ++i) {
		Interval newGuy = intervals[i];

		/* see if he stomps on someone (inefficient for now)
			... due to the way these are inserted, there should be no chance
				for doubmach/machine.hle stompage */
		bool leftBreak=false, rightBreak=false;

		/* we'll collect the split intervals and stuff into this temporary
			list then append to the flat list at the end */
		vector<Interval> tmpList;

		for(auto iter = flatList.begin(); iter != flatList.end();) {
			
			Interval oldGuy = *iter;

			// case:
			// oldGuy: [			]
			// newGuy:	[   ]
			// result: [ ][   ][	]
			if(!leftBreak && !rightBreak && oldGuy.contains(newGuy)) {
				/* left section */
				if(newGuy.left > oldGuy.left) {
					Interval tmp = oldGuy;
					tmp.right = newGuy.left;
					tmpList.push_back(tmp);
				}
				
				/* right section */
				if(oldGuy.right > newGuy.right) {
					Interval tmp = oldGuy;
					tmp.left = newGuy.right;
					tmpList.push_back(tmp);
				}
  
				/* if contained in an interval in flatlist, it's impossible to
					be in another interval, stop early */
				leftBreak = rightBreak = true;
				iter = flatList.erase(iter);
			}
			else
			// case:
			// oldGuy: [			]
			// newGuy:			[   ]
			// result: [		 ][   ]
			if(!leftBreak && oldGuy.contains(newGuy.left)) {
				/* left section */
				if(newGuy.left > oldGuy.left) {
					Interval tmp = oldGuy;
					oldGuy.right = newGuy.left;
					tmpList.push_back(Interval(tmp));
				}

				/* since flatList is non-overlapping, the LHS cannot be
					contained in another interval */
				iter = flatList.erase(iter);
				leftBreak = true;
			}
			else
			// case:
			// oldGuy:   [			]
			// newGuy: [   ]
			// result: [   ][		 ]
			if(!rightBreak && oldGuy.contains(newGuy.right)) {
				if(oldGuy.right > newGuy.right) {
					Interval tmp = oldGuy;
					oldGuy.left = newGuy.right;
					tmpList.push_back(Interval(tmp));
				}

				iter = flatList.erase(iter);
				rightBreak = true;
			}
			else {
				iter++;
			}

			if(leftBreak && rightBreak) {
				break;
			}
		}

		flatList.push_back(newGuy);
		flatList.insert(flatList.end(), tmpList.begin(), tmpList.end());
	}

	// STEP 3: sort the list by starting address
	intervals = flatList;
	sortByStartAddr();

	searchPrepared = true;
}

// recursive helper for searchFast()
bool IntervalMgr::searchFast(uint64_t target, int i, int j, Interval **result)
{
	/* base case */
	if(i==j) {
		if(intervals[i].contains(target)) {
			*result = &(intervals[i]);
			return true;
		}

		return false;
	}

	/* split */
	int idxMid = i + ((j - i) / 2);
	Interval intMid = intervals[idxMid];

	/* binary search */
	if(target < intMid.right) {
		return searchFast(target, i, idxMid, result);
	}
	else {
		return searchFast(target, idxMid+1, j, result);
	}
}

// searchFast() main API call
bool IntervalMgr::searchFast(uint64_t target, Interval **result)
{
	if(intervals.size() == 0) {
		return false;
	}

	return searchFast(target, 0, intervals.size()-1, result);
}

// return the smallest sized interval the target is a member of
bool IntervalMgr::search(uint64_t target, Interval &result)
{
	uint64_t length_lowest = 0;

	for(auto i=intervals.begin(); i!=intervals.end(); ++i) {
		Interval ival = *i;
		if(ival.contains(target)) {
			if(!length_lowest || ival.length < length_lowest) {
				length_lowest = ival.length;
				result = ival;
			}
		}
	}

	return length_lowest != 0;
}

// arrange the intervals into a tree structure
// intervals towards root are enveloping intervals
// intervals towards branches are enveloped intervals
// 
// returns a vector of the root nodes of the tree
//
// but instead (for now) we'll use the simple O(n^2) algo:
// for each interval, scan over the entire list of intervals
//
// return (by copy) a vector of pointers (within our internal intervals) of the
// parents
// 
vector<Interval *> IntervalMgr::findParentChild()
{
	vector<Interval *> listRoot;
	
	for(unsigned int i=0; i<intervals.size(); ++i) {
		//printf("interval @%p ", &(intervals[i]));
		//intervals[i].print();
	}

	/* we search for children backwards in the vector

		this means that priority to being enveloped is given to tags listed later

		priority to enveloping is given to tags listed earlier
	*/
	for(int i=intervals.size()-1; i>=0; --i) {
		/* smallest interval yet */
		unsigned int parent_i = -1;
		uint64_t parent_length = 0;

		for(int j=intervals.size()-1; j>=0; --j) {
			/* can't envelop yourself */
			if(i==j) continue;
			/* if it envelopes */
			if(!intervals[j].contains(intervals[i])) continue;
			/* is it the smallest we've seen so far? */
			if(parent_length && !(intervals[j].length < parent_length)) continue;
			/* is there already an enveloping relationship?
				(happens when taggers specify two tags with same interval) */
			Interval *candidateChild = &intervals[i];
			Interval *candidateParent = &intervals[j];
			if(candidateChild->childCheck(candidateParent)) continue;
			/* ok, update best (tightest enveloping) parent */
			parent_i = j;
			parent_length = candidateParent->length;
		}

		/* if parent found, add to parent */
		if(parent_length) {
			#ifdef INTERVAL_MGR_DEBUG
			printf("%s enveloped by %s\n",
			  intervals[i].data_string.c_str(),
			  intervals[parent_i].data_string.c_str()
			);
			#endif
			intervals[parent_i].childAdd(&(intervals[i]));
		}
		/* if not parent found, add to root */
		else {
			#ifdef INTERVAL_MGR_DEBUG
			printf("%s stands alone\n",
			  intervals[i].data_string.c_str()
			);
			#endif
			listRoot.push_back(&(intervals[i]));
		}
	}

	// STEP 3: sort the list by starting address
	std::sort(listRoot.begin(), listRoot.end(), compareByStartAddrP); 

	// and the children
	for(unsigned int i=0; i<listRoot.size(); ++i) {
		listRoot[i]->childSortByAddr();
	}

	return listRoot;
}

void IntervalMgr::print()
{
	for(unsigned int i=0; i<intervals.size(); ++i) {
		intervals[i].print();
	}
}

/*****************************************************************************/
/* TESTS */
/*****************************************************************************/

#ifdef TEST1
// g++ -std=c++11 -DTEST1 IntervalMgr.cxx -o test -lre2 -lautils
int main(int ac, char **av)
{
	int rc = -1;
	IntervalMgr mgr;
	vector<Interval *> roots;

	if(ac > 1) {
		printf("loading %s as a tags file\n", av[1]);
		if(mgr.readFromFile(av[1])) {
			printf("ERROR: readFromFile()\n");
			goto cleanup;
		}
	}
	else {
		printf("ERROR: expected arguments\n");
		goto cleanup;
	}

	roots = mgr.findParentChild();

	for(auto iter = roots.begin(); iter != roots.end(); iter++)
		(*iter)->print(true);

	rc = 0;
	cleanup:
	return rc;
}
#endif

#ifdef TEST2
void insert_dfs(Fl_Tree *tree, Fl_Tree_Item *item, Interval *tag)
{
	/* insert current guy */
	const char *tagName = tag->data_string.c_str();
	Fl_Tree_Item *itemNew = tree->add(item, tagName);
	itemNew->close();

	/* insert all his children */
	vector<Interval *> children = tag->childRetrieve();
	for(auto iter = children.begin(); iter != children.end(); iter++) {
		Interval *childTag = (*iter);
		insert_dfs(tree, itemNew, childTag);
	}
}

// g++ -std=c++11 -DTEST2 IntervalMgr.cxx -o test -lre2 -lautils -lfltk `fltk-config --use-images --ldstaticflags`
int main(int ac, char **av)
{
	int rc = -1;
	IntervalMgr mgr;
	vector<Interval *> roots;
	Fl_Tree *tree;
	Fl_Tree_Item *item;
	Fl_Double_Window *win;

	if(ac > 1) {
		printf("loading %s as a tags file\n", av[1]);
		if(mgr.readFromFile(av[1])) {
			printf("ERROR: readFromFile()\n");
			goto cleanup;
		}
	}
	else {
		printf("ERROR: expected arguments\n");
		goto cleanup;
	}

	roots = mgr.findParentChild();

	//Fl::scheme("gtk+");
	win = new Fl_Double_Window(250, 400, "IntervalMgr Test");
	win->begin();
	// Create the tree
	tree = new Fl_Tree(10, 10, win->w()-20, win->h()-20);
	tree->showroot(0);				// don't show root of tree
	//tree->callback(TreeCallback);		// setup a callback for the tree
	tree->end();
	win->end();
	win->resizable(win);
	/* can optionally pass argc, argv here, see Fl::args() */
	win->show();

	/* print the hierarchy textually */
	for(auto iter = roots.begin(); iter != roots.end(); iter++)
		(*iter)->print(true);

	/* just put the roots into the tree */
	for(auto iter = roots.begin(); iter != roots.end(); iter++) {
		Interval *tag = *iter;
		insert_dfs(tree, tree->root(), tag);
	}
	
	rc = Fl::run();

	cleanup:
	return rc;
}
#endif
