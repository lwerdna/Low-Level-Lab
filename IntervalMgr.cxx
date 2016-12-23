#include <stdint.h>
#include <stdlib.h>

#include <vector>
#include <algorithm>
using namespace std;

#include "IntervalMgr.h"

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

void Interval::print()
{
    printf("interval@%p [%016llX,%016llX)", this, left, right);
    if(data_type) printf(" data: ");
    switch(data_type) {
        case 1: printf("%p", data_void_ptr); break;
        case 2: printf("0x%08X", data_u32); break;
        case 3: printf("\"%s\"", data_string.c_str()); break;
    }

    if(children.size() > 0) {
        printf(" (%ld children)", children.size());
    }

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

void IntervalMgr::IntervalMgr::add(Interval iv)
{
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
    //         intervals will overwrite and thus have higher priority
    //
    //         insertion will split pre-existing into non-overlapping intervals
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
            // oldGuy: [            ]
            // newGuy:    [   ]
            // result: [ ][   ][    ]
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
            // oldGuy: [            ]
            // newGuy:            [   ]
            // result: [         ][   ]
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
            // oldGuy:   [            ]
            // newGuy: [   ]
            // result: [   ][         ]
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
    //printf("searchFast(%d, %d, %d)\n", target, i, j);

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

    for(unsigned int i=0; i<intervals.size(); ++i) {

        /* find smallest enveloping interval */
        unsigned int parent_i = -1;
        uint64_t parent_length = 0;

        for(unsigned int j=0; j<intervals.size(); j++) {
            if(i==j) continue;

            if(intervals[j].contains(intervals[i])) {
                if(!parent_length || (intervals[j].length < parent_length)) {
                    parent_i = j;
                    parent_length = intervals[j].length;
                }
            }
        }

        /* add to list, add to child if parent found */
        if(parent_length) {
            intervals[parent_i].childAdd(&(intervals[i]));
        }
        else {
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

//#define MAIN_TEST
#ifdef MAIN_TEST
int main(int ac, char **av)
{
    IntervalMgr im;

    /* middle case */
    im.add(5, 0x35, 0xAA);
    im.add(8, 0x25, 0xBB);

    /* left intersect */
    im.add(5, 0x35, 0xCC);
    im.add(2, 0x25, 0xDD);

    /* right intersect */
    im.add(0x8, 0x50, 0xEE);
    im.add(0x16, 0x60, 0xFF);

    im.searchFastPrep();
    im.print();

    for(int i=0; i<200; ++i) {
        Interval *ival = NULL;
        if(im.searchFast(i, &ival)) {
            printf("search(%X) returns: %08X\n", i, ival->data_u32);
        }
        else {
            printf("search(%X) misses\n", i);
        }
    }

    //
    im.clear();
    im.add(1, 5, 0xAA);
    im.add(2, 3, 0xBB);
    im.add(10, 20, 0xCC);
    im.add(12, 18, 0xDD);
    im.add(30, 40, 0xEE);
    im.add(32, 38, 0xFF);
    vector<Interval *> tree = im.arrangeIntoTree();

    for(int i=0; i<tree.size(); ++i) {
        tree[i]->print();
    }
}
#endif
