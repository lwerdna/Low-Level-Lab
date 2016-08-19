#include <stdint.h>

#include <vector>
#include <algorithm>
using namespace std;

#include "IntervMgr.h"

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

/*****************************************************************************/
/* interval class */
/*****************************************************************************/
Interval::Interval(uint64_t left_, int right_)
{
    left = left_;
    right = right_; // [,)
    length = right - left;
    data = 0;
}

Interval::Interval(uint64_t left_, int right_, uint32_t data_)
{
    left = left_;
    right = right_; // [,)
    length = right - left;
    data = data_;
}

bool Interval::contains(uint64_t addr)
{
    return (addr >= left && addr < right);
}

bool Interval::contains(Interval &ival)
{
    return contains(ival.left) && contains(ival.right);
}

bool Interval::intersects(Interval &ival)
{
    return contains(ival.left) || contains(ival.right);
}

void Interval::print()
{
    printf("[%016llX,%016llX) with data: %08X\n", left, right, data);
}

/*****************************************************************************/
/* interval manager class */
/*****************************************************************************/

void IntervMgr::add(uint64_t left, uint64_t right, int32_t color)
{
    intervals.push_back(Interval(left, right, color));
}

/* sort by interval start address */
void IntervMgr::sortByStartAddr()
{
    std::sort(intervals.begin(), intervals.end(), compareByStartAddr); 
}

/* sort by interval lengths */
void IntervMgr::sortByLength()
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

void IntervMgr::searchFastPrep()
{
    // STEP 1: sorts intervals by descending size, overlap is still possible here
    sortByLength();
   
    // STEP 2: insert into new list the largest intervals first (smaller
    //         intervals will overwrite and thus have higher priority
    //
    //         insertion will split pre-existing into non-overlapping intervals
    vector<Interval> flatList;

    // insert into sorted (by start addr) list (removing overlap)
    for(int i=0; i<intervals.size(); ++i) {
        Interval newGuy = intervals[i];

        /* see if he stomps on someone (inefficient for now)
            ... due to the way these are inserted, there should be no chance
                for double stompage */
        bool leftBreak=false, rightBreak=false;
        bool splitInsert=false;

        /* we'll collect the split intervals and stuff into this temporary
            list then append to the flat list at the end */
        vector<Interval> tmpList;

        for(auto iter = flatList.begin(); iter != flatList.end();) {
            
            Interval oldGuy = *iter;

            oldGuy.print();
            newGuy.print();

            // case:
            // oldGuy: [            ]
            // newGuy:    [   ]
            // result: [ ][   ][    ]
            if(!leftBreak && !rightBreak && oldGuy.contains(newGuy)) {
                /* left section */
                if(newGuy.left > oldGuy.left) {
                    //printf("a:\n");
                    tmpList.push_back(Interval(oldGuy.left, newGuy.left, oldGuy.data));
                }
                
                /* middle section */
                //printf("b:\n");
                tmpList.push_back(newGuy);

                /* right section */
                if(oldGuy.right > newGuy.right) {
                    //printf("c:\n");
                    tmpList.push_back(Interval(newGuy.right, oldGuy.right, oldGuy.data));
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
                    tmpList.push_back(Interval(oldGuy.left, newGuy.left, oldGuy.data));
                }
                tmpList.push_back(newGuy);

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
                    tmpList.push_back(Interval(newGuy.right, oldGuy.right, oldGuy.data));
                }
                tmpList.push_back(newGuy);

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

        /* if we didn't collide with any other intervals, just insert normal */
        if(tmpList.size() == 0) {
            printf("inserting by default\n");
            flatList.push_back(newGuy);
        }
        else {
            flatList.insert(flatList.end(), tmpList.begin(), tmpList.end());
        }
    }

    // STEP 3: sort the list by starting address
    intervals = flatList;
    sortByStartAddr();

    searchPrepared = true;
}

bool IntervMgr::searchFast(uint64_t target, int i, int j)
{
    return true;
}

bool IntervMgr::searchFast(uint64_t target)
{
    return true;
}

void IntervMgr::print()
{
    for(int i=0; i<intervals.size(); ++i) {
        Interval intv = intervals[i];
        intv.print();
    }
}

//int main(int ac, char **av)
//{
//    IntervMgr im;
//
//    /* middle case */
//    im.add(5, 50, 0xAA);
//    im.add(8, 25, 0xBB);
//
//    /* left intersect */
//    im.add(5, 50, 0xCC);
//    im.add(2, 25, 0xDD);
//
//    /* right intersect */
//    im.add(0x8, 0x50, 0xEE);
//    im.add(0x16, 0x60, 0xFF);
//
//    im.searchFastPrep();
//    im.print();
//
//}
