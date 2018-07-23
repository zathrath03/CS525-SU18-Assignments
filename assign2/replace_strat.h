#include <stdbool.h>

//LRU
int lruFindToReplace(BM_BufferPool *const bm);
void lruFree(BM_BufferPool *const bm);
void lruPin(BM_BufferPool * bm,int frameNumber);
BM_PageHandle * lruReplace(BM_BufferPool *const bm);
void lruInit(BM_BufferPool * bm);

void clockPin(BM_BufferPool *const bm, PageNumber pageNumber);

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
