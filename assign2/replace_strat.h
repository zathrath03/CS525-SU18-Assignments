#include <stdbool.h>

typedef struct listNode {
    BM_PageHandle pageHandle;
    struct listNode *nextNode;
} listNode;

typedef struct FIFO {
    listNode *head;
    listNode *tail;
} fifo;

typedef struct LRU {
    listNode *head;
    listNode *tail;
} lru;

typedef struct clock {
    bool *wasReferencedArray;
} clock;

typedef struct LFU {
    //I have no idea what you need in this struct
} lfu;