class Interval
{
    public:
    Interval(uint64_t addr, int length);
    Interval(uint64_t addr, int length, uint32_t data);
    bool contains(uint64_t addr);
    bool contains(Interval &ival);
    bool intersects(Interval &ival);
    uint64_t left;
    uint64_t right;
    int length;
    uint32_t data;

    void print();
};

class IntervMgr
{
    vector<Interval> intervals;
    bool searchPrepared;

    public:
    void add(uint64_t left, uint64_t right, int32_t color);

    void sortByStartAddr();
    void sortByLength();

    void searchFastPrep();
    bool searchFast(uint64_t target, int i, int j, uint32_t *data);
    bool searchFast(uint64_t target, uint32_t *data);

    void print();
};
