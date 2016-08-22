class Interval
{
    private:
    bool bOnDestructionFree=false;
    vector<Interval *> children;

    public:
    void *data_void_ptr = NULL;
    uint32_t data_u32;
    
    Interval(uint64_t addr, int length);
    Interval(uint64_t addr, int length, void *data);
    Interval(uint64_t addr, int length, uint32_t data);
    ~Interval();

    void setDestructorFree(void);

    bool contains(uint64_t addr);
    bool contains(Interval &ival);
    bool intersects(Interval &ival);

    void childAdd(Interval *);
    void childSortByAddr(void);
    void childSortByLength(void);
    vector<Interval *> childRetrieve(void);

    uint64_t left;
    uint64_t right;
    uint64_t length;

    void print();
};

class IntervalMgr
{
    vector<Interval> intervals;
    bool searchPrepared=false;
    bool bOnDestructionFree=false;
    
    bool searchFast(uint64_t target, int i, int j, Interval **result);

    public:
    ~IntervalMgr();

    /* you can add various things with the integer intervals with simple over-
        loaded methods here */
    Interval * add(uint64_t left, uint64_t right, void *data);
    Interval * add(uint64_t left, uint64_t right, uint32_t data);
    void clear(void);

    void sortByStartAddr();
    void sortByLength();

    void searchFastPrep();
    bool searchFast(uint64_t target, Interval **result);

    vector<Interval *> arrangeIntoTree();

    void setDestructorFree(void);

    void print();
};
