#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <io.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "replace_strat.h"

//PROTOTYPES
RC initBufferPoolInfo(BM_BufferPool * bm,ReplacementStrategy strategy,void * stratData);
RC initRelpacementStrategy(BM_BufferPool * bm,ReplacementStrategy strategy,void *stratData);
RC freeReplacementStrategy(BM_BufferPool *const bm);

//Prototypes helper functions
int findFrameNumber(BM_BufferPool * bm, PageNumber pageNumber);
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
    return initRelpacementStrategy(bm, strategy, stratData);
}


RC initRelpacementStrategy(BM_BufferPool * bm,ReplacementStrategy strategy,void *stratData){
    switch(strategy){
    case RS_FIFO:
        break;
    case RS_LRU:
        lruInit(bm);
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
    ReplacementStrategy strategy = bm->strategy;
    switch(strategy){
    case RS_FIFO:
        break;
    case RS_LRU:
        lruFree(bm);
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

typedef struct BM_PoolInfo {
    BM_PageHandle *poolMem_ptr; //points to the start of the pool in memory
    int numReadIO; //track number of pages read from disk since initialization
    int numWriteIO; //track number of pages written to disk since initialization
    bool *isDirtyArray; //array that tracks the dirty state of each frame
    int *fixCountArray; //array that tracks the fixCount of each frame
    void *rplcStratStruct; //contains data needed for replacement strategy
} BM_PoolInfo;
*********************************************************************/
RC forceFlushPool(BM_BufferPool *const bm){
    //get memory address of the first page
    BM_PoolInfo *poolInfo = bm->mgmtData;
    BM_PageHandle *frame_ptr = poolInfo->poolMem_ptr;

    //search through the pages stored in the buffer pool for the page of interest
    for (int i = 0; i < bm->numPages; i++){
        frame_ptr += i; //address of the ith frame
        if ((poolInfo->fixCountArray[i] == 0) && (poolInfo->isDirtyArray[i]==true)){
            //once the page with dirty bit true and fix count =0 is found,
            //write its information to the disk
            (poolInfo->isDirtyArray[i]) = false;
            (poolInfo->numWriteIO)++;
            return RC_OK;
        }
    }
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
    int frameNum = findFrameNumber(bm,pageNum);
    switch(bm->strategy) {
    case RS_FIFO:
        break;
    case RS_LRU:
        lruPin(bm,frameNum);
        break;
    case RS_CLOCK:
        clockPin(bm,frameNum);
        break;
    case RS_LFU:
        break;
    case RS_LRU_K:
        break;
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
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    if(!page)
        return RC_BM_PAGE_NOT_FOUND;

    //initialize variables
    int frameNum = 0;

    //search through the pages stored in the buffer pool for the page of interest
    if((frameNum = findFrameNumber(bm, page->pageNum)) == -1)
        return RC_BM_PAGE_NOT_FOUND;
    bm->mgmtData->isDirtyArray[frameNum] = true;

    return RC_OK;
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

    //search through the pages stored in the buffer pool for the page of interest
    int frameNum = -1;
    if((frameNum = findFrameNumber(bm, page->pageNum)) == -1)
        return RC_BM_PAGE_NOT_FOUND;
    bm->mgmtData->isDirtyArray[frameNum] = false;

    return returnCode;
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
    //Create a frameContents array to hold all of the page numbers we need
    PageNumber frameContents[bm->numPages];
    BM_PoolInfo *poolInfo = bm->mgmtData;
    BM_PageHandle *frame_ptr = poolInfo->poolMem_ptr;

  	//iterate through the frames
    for (int i = 0; i < bm->numPages; i++){
      	//Stores frame pointer into pageHandle in the BM_pageHandle struct
      	BM_PageHandle *pageHandle = (frame_ptr+i);
      	//if the pageHandle doesn't exist then set it to NO_PAGE
      	if (!pageHandle)
          frameContents[i] = NO_PAGE;
      	else //If pageHandle exists, then set frame contents at the ith position to page number of that pageHandle
          frameContents[i] = pageHandle->pageNum;
    }
    return (PageNumber*) *frameContents;
}

/*********************************************************************
getDirtyFlags returns an array of bools (of size
numPages) where the ith element is TRUE if the page stored in the ith
page frame is dirty. Empty page frames are considered as clean.
*********************************************************************/
bool *getDirtyFlags (BM_BufferPool *const bm){
    return bm->mgmtData->isDirtyArray;
}
/*********************************************************************
getFixCounts returns an array of ints (of size numPages)
where the ith element is the fix count of the page stored in the ith
page frame. Return 0 for empty page frames.
*********************************************************************/
int *getFixCounts (BM_BufferPool *const bm){
    return bm->mgmtData->fixCountArray;
}

/*********************************************************************
getNumReadIO returns the number of pages that have been
read from disk since a buffer pool has been initialized. You code is
responsible to initializing this statistic at pool creating time and
update whenever a page is read from the page file into a page frame.
*********************************************************************/
int getNumReadIO (BM_BufferPool *const bm){
    return bm->mgmtData->numReadIO;
}

/*********************************************************************
getNumWriteIO returns the number of pages written to the page file
since the buffer pool has been initialized.
*********************************************************************/
int getNumWriteIO (BM_BufferPool *const bm){
    return bm->mgmtData->numWriteIO;
}

/*********************************************************************
*
*                        HELPER FUNCTIONS
*
*********************************************************************/

/*********************************************************************
Helper function to find the frame number for given pageNumber in
a buffer pool
*********************************************************************/
int findFrameNumber(BM_BufferPool * bm, PageNumber pageNumber){
    //get memory address of the first pagefor
    BM_PageHandle *frame_ptr = bm->mgmtData->poolMem_ptr;

    //search through the pages stored in the buffer pool for the page of interest
    for (int i = 0; i < bm->numPages; i++){
        //frame_ptr+i is the address of the ith frame
        if ((frame_ptr+i)->pageNum == pageNumber)
            return i;
    }
    return -1;
}
