#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

// Include return codes and methods for logging errors
#include "dberror.h"
#include <stdbool.h>

// Replacement Strategies
typedef enum ReplacementStrategy {
  RS_FIFO = 0,
  RS_LRU = 1,
  RS_CLOCK = 2,
  RS_LFU = 3,
  RS_LRU_K = 4
} ReplacementStrategy;

// Data Types and Structures
typedef int PageNumber;
#define NO_PAGE -1

typedef struct BM_PageHandle {
  PageNumber pageNum;
  char *data;
} BM_PageHandle;

typedef struct BM_PoolInfo {
    BM_PageHandle *poolMem_ptr; //points to the start of the pool in memory
    int numReadIO; //track number of pages read from disk since initialization
    int numWriteIO; //track number of pages written to disk since initialization
    bool *isDirtyArray; //array that tracks the dirty state of each frame
    int *fixCountArray; //array that tracks the fixCount of each frame
    int *frameContent; //array that tracks the pageNumber for every frame
    void *rplcStratStruct; //contains data needed for replacement strategy
} BM_PoolInfo;

typedef struct BM_BufferPool {
  char *pageFile;
  int numPages;
  ReplacementStrategy strategy;
  BM_PoolInfo *mgmtData; // use this one to store the bookkeeping info your buffer
                  // manager needs for a buffer pool
} BM_BufferPool;

// convenience macros
#define MAKE_POOL()					\
  ((BM_BufferPool *) malloc (sizeof(BM_BufferPool)))

#define MAKE_POOL_INFO()            \
    ((BM_PoolInfo *) malloc (sizeof(BM_PoolInfo)))

#define MAKE_PAGE_HANDLE()				\
  ((BM_PageHandle *) malloc (sizeof(BM_PageHandle)))

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
		  const int numPages, ReplacementStrategy strategy,
		  void *stratData);
RC shutdownBufferPool(BM_BufferPool *const bm);
RC forceFlushPool(BM_BufferPool *const bm);

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page);
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page);
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page);
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page,
	    const PageNumber pageNum);

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bm);
bool *getDirtyFlags (BM_BufferPool *const bm);
int *getFixCounts (BM_BufferPool *const bm);
int getNumReadIO (BM_BufferPool *const bm);
int getNumWriteIO (BM_BufferPool *const bm);

#endif
