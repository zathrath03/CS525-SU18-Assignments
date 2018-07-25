#include <stdbool.h>

//FIFO
void fifoInit(BM_BufferPool *bm);
void fifoFree(BM_BufferPool *const bm);
void fifoPin(BM_BufferPool *bm, int frameNum);
BM_PageHandle * fifoReplace(BM_BufferPool *const bm);

//LRU
int lruFindToReplace(BM_BufferPool *const bm);
void lruFree(BM_BufferPool *const bm);
void lruPin(BM_BufferPool * bm,int frameNumber);
BM_PageHandle * lruReplace(BM_BufferPool *const bm);
void lruInit(BM_BufferPool * bm);

//Clock
void clockInit(BM_BufferPool *bm);
void clockFree(BM_BufferPool *const bm);
void clockPin(BM_BufferPool *const bm, int frameNum);
BM_PageHandle * clockReplace(BM_BufferPool *const bm);

//LFU
void lfuInit(BM_BufferPool *bm);
void lfuFree(BM_BufferPool *const bm);
void lfuPin(BM_BufferPool *const bm, int frameNum);
BM_PageHandle * lfuReplace(BM_BufferPool *const bm);

typedef struct listNode {
    //BM_PageHandle pageHandle;
    int frameNum;
    struct listNode *nextNode;
} listNode;

typedef struct RS_FIFOInfo {
    listNode *head;
    listNode *tail;
} RS_FIFOInfo;

typedef struct RS_ClockInfo {
    bool *wasReferencedArray;
    int curPage;
} RS_ClockInfo;

typedef struct RS_LFUInfo {
    int *frequency;
    int lfuIndex;
} RS_LFUInfo;
