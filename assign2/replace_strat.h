#include <stdbool.h>

typedef struct listNode {
    BM_PageHandle pageHandle;
    struct listNode *nextNode;
} listNode;

typedef struct RS_FIFOInfo {
    listNode *head;
    listNode *tail;
} RS_FIFOInfo;

typedef struct RS_LRUInfo {
    listNode *head;
    listNode *tail;
} RS_LRUInfo;

typedef struct RS_ClockInfo {
    bool *wasReferencedArray;
    int curPage;
} RS_ClockInfo;

typedef struct RS_LFUInfo {
    //I have no idea what you need in this struct
} RS_LFUInfo;
