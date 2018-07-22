#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "replace_strat.h"

//PROTOTYPES
RC initBufferPoolInfo(BM_BufferPool * bm,ReplacementStrategy strategy,void * stratData);
RC initRelpacementStrategy(BM_PoolInfo * pi,ReplacementStrategy strategy,void *stratData);
/*********************************************************************
*
*             BUFFER MANAGER INTERFACE POOL HANDLING
*
*********************************************************************/

/*********************************************************************
initBufferPool creates a new buffer pool with numPages page frames
using the page replacement strategy strategy. The pool is used to
cache pages from the page file with name pageFileName. Initially, all
page frames should be empty. The page file should already exist, i.e.,
this method should not generate a new page file. stratData can be used
to pass parameters for the page replacement strategy. For example, for
LRU-k this could be the parameter k.
*********************************************************************/
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
    const int numPages, ReplacementStrategy strategy, void *stratData){
    //check BM_BufferPool has space allocated
    if(!bm){
        return RC_BM_NOT_ALLOCATED;
    }
    //check we have fileName
    if(!pageFileName){
        return RC_NO_FILENAME;
    }
    bm->pageFile = (char *)pageFileName;
    //check if the pageFile is a valid one
    if(access(pageFileName, 0) == -1){
        return RC_FILE_NOT_FOUND;
    }
    //check the number of pages
    if(numPages<1){
        return RC_INVALID_PAGE_NUMBER;
    }
    bm->numPages = numPages;
    bm->strategy = strategy;
    return initBufferPoolInfo(bm,strategy,stratData);
}

RC initBufferPoolInfo(BM_BufferPool * bm,ReplacementStrategy strategy,void * stratData){
    BM_PoolInfo * pi = MAKE_POOL_INFO();
    //initialize PoolInfo values
    pi->numReadIO = 0;
    pi->numWriteIO = 0;

    //Set the isDirtyArray,fixCountArray to false,0
    bool isDirtyArray[bm->numPages];
    for(int i =0; i <bm->numPages;i++){
        isDirtyArray[i]=false;
    }
    pi->isDirtyArray = isDirtyArray;
    int fixCountArray[bm->numPages];
    for(int i; i<bm->numPages;i++){
        fixCountArray[i]=0;
    }
    pi->fixCountArray = fixCountArray;

    //allocate memory for pageFrames
    pi->poolMem_ptr = malloc(sizeof(BM_PageHandle)*bm->numPages);
    bm->mgmtData = pi;
    return initRelpacementStrategy(pi, strategy, stratData);
}


RC initRelpacementStrategy(BM_PoolInfo * pi,ReplacementStrategy strategy,void *stratData){
    switch(strategy){
    case RS_FIFO:
        break;
    case RS_LRU:
        break;
    case RS_CLOCK:
        break;
    case RS_LFU:
        break;
    case RS_LRU_K:
        break;
    default:
        printf("UNKNOWN REPLACEMENT STRATEGY");
        return RC_RS_UNKNOWN;
    }
    return RC_OK;
}

/*********************************************************************
shutdownBufferPool destroys a buffer pool. This method should free up
all resources associated with buffer pool. For example, it should free
the memory allocated for page frames. If the buffer pool contains any
dirty pages, then these pages should be written back to disk before
destroying the pool. It is an error to shutdown a buffer pool that has
pinned pages.
Resources such as:
- pagesFrame ptrs
- struct for poolInfo
- struct for BufferManager
*********************************************************************/
RC shutdownBufferPool(BM_BufferPool *const bm)
{
    RC rc;
    if((rc = forceFlushPool(bm))!=RC_OK )
    {
        return rc;
    }
    //free up space from pageFrames
    BM_PoolInfo *poolInfo = bm->mgmtData;
    BM_PageHandle *frame_ptr = poolInfo->poolMem_ptr;

    for (int i = 0; i < bm->numPages; i++)
    {
        frame_ptr += i; //address of the ith frame
        free(frame_ptr);
    }
    if((rc = freeReplacementStrategy(bm))!=RC_OK){
        return rc;
    }
    free(poolInfo);
    free(bm);
    return RC_OK;
}


/********************************************************************
Free up any space allocated for a replacement strategy
*********************************************************************/
RC freeReplacementStrategy(BM_BufferPool *const bm){
    switch(strategy){
    case RS_FIFO:
        break;
    case RS_LRU:
        break;
    case RS_CLOCK:
        break;
    case RS_LFU:
        break;
    case RS_LRU_K:
        break;
    default:
        printf("UNKNOWN REPLACEMENT STRATEGY");
        return RC_RS_UNKNOWN;
    }
    return RC_OK;
}

/*********************************************************************
forceFlushPool causes all dirty pages (with fix count 0) from the
buffer pool to be written to disk.
*********************************************************************/
RC forceFlushPool(BM_BufferPool *const bm){

    return RC_OK;
}

/*********************************************************************
*
*                BUFFER MANAGER INTERFACE ACCESS PAGES
*
*********************************************************************/

/*********************************************************************
pinPage pins the page with page number pageNum. The buffer manager is
responsible to set the pageNum field of the page handle passed to the
method. Similarly, the data field should point to the page frame the
page is stored in (the area in memory storing the content of the
page).
*********************************************************************/
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const
    PageNumber pageNum){

    //When you pin the page, if bm->strategy is clock, I need you to
    //set the bm->mgmtData->rplcStratStruct->wasReferencedArray to true
    //for the frame you're pinning the page into
    //frameNum is a place holder for however you're going to track what
    //frame you're putting the data into
    int frameNum = 0;
    if(bm->strategy == RS_CLOCK){
        BM_PoolInfo *poolInfo = bm->mgmtData;
        RS_ClockInfo *clockInfo = poolInfo->rplcStratStruct;

        clockInfo->wasReferencedArray[frameNum] = true;
    }

    return RC_OK;
}

/*********************************************************************
unpinPage unpins the page page. The pageNum field of page should be
used to figure out which page to unpin.
*********************************************************************/
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){

    return RC_OK;
}

/*********************************************************************
markDirty marks a page as dirty.
*********************************************************************/
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page){
    //get memory address of the first page
    BM_PoolInfo *poolInfo = bm->mgmtData;
    BM_PageHandle *frame_ptr = poolInfo->poolMem_ptr;

    //search through the pages stored in the buffer pool for the page of interest
    for (int i = 0; i < bm->numPages; i++){
        frame_ptr += i; //address of the ith frame
        if (frame_ptr->pageNum == page->pageNum){
            //mark page as dirty
            poolInfo->isDirtyArray[i] = 1;
            return RC_OK;
        }
    }

    return RC_BM_PAGE_NOT_FOUND;
}

/*********************************************************************
forcePage should write the current content of the page back to the
page file on disk.
*********************************************************************/
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page){
    SM_FileHandle fHandle;
    RC returnCode = RC_OK;

    if((returnCode = openPageFile(bm->pageFile, &fHandle)) != RC_OK)
        return returnCode;

    if((returnCode = writeBlock(page->pageNum, &fHandle, page->data)) != RC_OK)
        return returnCode;

    if((returnCode = closePageFile(&fHandle)) != RC_OK)
        return returnCode;

    //get memory address of the first page
    BM_PoolInfo *poolInfo = bm->mgmtData;
    BM_PageHandle *frame_ptr = poolInfo->poolMem_ptr;

    //search through the pages stored in the buffer pool for the page of interest
    for (int i = 0; i < bm->numPages; i++){
        frame_ptr += i; //address of the ith frame
        if (frame_ptr->pageNum == page->pageNum){
            //mark page as clean
            poolInfo->isDirtyArray[i] = 0;
            return RC_OK;
        }
    }

    return RC_OK;
}

/*********************************************************************
*
*                        STATISTICS INTERFACE
*
*********************************************************************/

/*********************************************************************
getFrameContents returns an array of PageNumbers (of size
numPages) where the ith element is the number of the page stored in
the ith page frame. An empty page frame is represented using the
constant NO_PAGE.
*********************************************************************/
PageNumber *getFrameContents (BM_BufferPool *const bm){

    return (PageNumber)0;
}

/*********************************************************************
getDirtyFlags returns an array of bools (of size
numPages) where the ith element is TRUE if the page stored in the ith
page frame is dirty. Empty page frames are considered as clean.
*********************************************************************/
bool *getDirtyFlags (BM_BufferPool *const bm){

    return true;
}

/*********************************************************************
getFixCounts returns an array of ints (of size numPages)
where the ith element is the fix count of the page stored in the ith
page frame. Return 0 for empty page frames.
*********************************************************************/
int *getFixCounts (BM_BufferPool *const bm){

    return 0;
}

/*********************************************************************
getNumReadIO returns the number of pages that have been
read from disk since a buffer pool has been initialized. You code is
responsible to initializing this statistic at pool creating time and
update whenever a page is read from the page file into a page frame.
*********************************************************************/
int getNumReadIO (BM_BufferPool *const bm){

    return 0;
}

/*********************************************************************
getNumWriteIO returns the number of pages written to the page file
since the buffer pool has been initialized.
*********************************************************************/
int getNumWriteIO (BM_BufferPool *const bm){

    return 0;
}
