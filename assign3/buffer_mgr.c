#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <io.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "replace_strat.h"

//PROTOTYPES
static RC initBufferPoolInfo(BM_BufferPool * bm,ReplacementStrategy strategy,void * stratData);
static RC initRelpacementStrategy(BM_BufferPool * bm,ReplacementStrategy strategy,void *stratData);
static RC freeReplacementStrategy(BM_BufferPool *const bm);
static BM_Frame * findEmptyFrame(BM_BufferPool *bm);

//Prototypes helper functions
static int findFrameNumber(BM_BufferPool * bm, PageNumber pageNumber);
static void pinRplcStrat(BM_BufferPool* bm, int frameNum);
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
                  const int numPages, ReplacementStrategy strategy, void *stratData) {
    //check BM_BufferPool has space allocated
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    //check we have fileName
    if(!pageFileName)
        return RC_NO_FILENAME;
    bm->pageFile = (char *)pageFileName;
    //check if the pageFile is a valid one
    if(access(pageFileName, R_OK|W_OK) == -1)
        return RC_FILE_NOT_FOUND;
    //check the number of pages
    if(numPages<1)
        return RC_INVALID_PAGE_NUMBER;
    bm->numPages = numPages;
    bm->strategy = strategy;
    return initBufferPoolInfo(bm,strategy,stratData);
}

static RC initBufferPoolInfo(BM_BufferPool * bm,ReplacementStrategy strategy,void * stratData) {
    BM_PoolInfo * pi = MAKE_POOL_INFO();
    if(!pi) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    //initialize PoolInfo values
    pi->numReadIO = 0;
    pi->numWriteIO = 0;

    //Set the isDirtyArray false
    pi->isDirtyArray = (bool *)calloc(bm->numPages, sizeof(bool));
    if(!pi->isDirtyArray) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }

    //Set the fixCountArray to 0
    pi->fixCountArray = (int *)calloc(bm->numPages,sizeof(int));
    if(!pi->fixCountArray) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }

    //Set the frameContent to NO_PAGE
    pi->frameContent = (int *)calloc(bm->numPages, sizeof(int));
    if(!pi->frameContent) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    memset(pi->frameContent, NO_PAGE, bm->numPages*(sizeof(int)));

    //allocate memory for pageFrames
    pi->poolMem_ptr = (BM_Frame *)calloc(bm->numPages, sizeof(BM_Frame));
    if(!pi->poolMem_ptr) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    bm->mgmtData = pi;
    return initRelpacementStrategy(bm, strategy, stratData);
}


static RC initRelpacementStrategy(BM_BufferPool * bm,ReplacementStrategy strategy,void *stratData) {
    //this is a default value and should be set
    // in the init functions below!!
    switch(strategy) {
    case RS_FIFO:
        fifoInit(bm);
        break;
    case RS_LRU:
        lruInit(bm);
        break;
    case RS_CLOCK:
        clockInit(bm);
        break;
    case RS_LFU:
        lfuInit(bm);
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
RC shutdownBufferPool(BM_BufferPool *const bm) {
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;

    RC rc;
    if((rc = forceFlushPool(bm))!=RC_OK ) {
        return rc;
    }
    //free up space from pageFrames
    BM_PoolInfo *poolInfo = bm->mgmtData;
    //free up pool info
    free(poolInfo->poolMem_ptr);
    poolInfo->poolMem_ptr=NULL;
    free(poolInfo->isDirtyArray);
    poolInfo->isDirtyArray=NULL;
    free(poolInfo->fixCountArray);
    poolInfo->fixCountArray=NULL;
    free(poolInfo->frameContent);
    poolInfo->frameContent=NULL;
    //free up replacement Strategy
    if((rc = freeReplacementStrategy(bm))!=RC_OK) {
        return rc;
    }

    free(poolInfo);
    poolInfo=NULL;
    return RC_OK;
}


/********************************************************************
Free up any space allocated for a replacement strategy
*********************************************************************/
static RC freeReplacementStrategy(BM_BufferPool *const bm) {

    ReplacementStrategy strategy = bm->strategy;
    switch(strategy) {
    case RS_FIFO:
        fifoFree(bm);
        break;
    case RS_LRU:
        lruFree(bm);
        break;
    case RS_CLOCK:
        clockFree(bm);
        break;
    case RS_LFU:
        lfuFree(bm);
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
RC forceFlushPool(BM_BufferPool *const bm) {
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;

    SM_FileHandle *fHandle = calloc(1, sizeof(SM_FileHandle));
    if(!fHandle) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    SM_PageHandle memPage = NULL;
    RC returnCode = RC_INIT;

    for(int i = 0; i < bm->numPages; i++) {
        if (bm->mgmtData->fixCountArray[i] == 0 && bm->mgmtData->isDirtyArray[i] == true) {
            memPage = (char*)(bm->mgmtData->poolMem_ptr + i);
            if((returnCode = openPageFile(bm->pageFile, fHandle)) != RC_OK) {
                free(fHandle);
                fHandle = NULL;
                return returnCode;
            }
            if((returnCode = writeBlock(bm->mgmtData->frameContent[i], fHandle, memPage))) {
                free(fHandle);
                fHandle = NULL;
                return returnCode;
            }
            if((returnCode = closePageFile(fHandle)) != RC_OK) {
                free(fHandle);
                fHandle = NULL;
                return returnCode;
            }
            bm->mgmtData->isDirtyArray[i] = false;
            bm->mgmtData->numWriteIO++;
        }
    }
    free(fHandle);
    fHandle = NULL;
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
NOTE:--------------------------------------------------------------
Please make sure to update the frameContent in the BM_PoolInfo on the
BM_BufferPool
*********************************************************************/
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const
            PageNumber pageNum) {

    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    if(!page)
        return RC_BM_PAGE_NOT_FOUND;
    if(pageNum <0)
        return RC_BM_PAGE_NOT_FOUND;

    //Finds the frame number if the page is already pinned in a frame
    int frameNum = findFrameNumber(bm, pageNum);
    //If the page exists in a frame
    if(frameNum != NO_PAGE) {
        //Increment the pin count for that frame
        bm->mgmtData->fixCountArray[frameNum]++;
        //Initialize the BM_PageHandle data
        page->pageNum = pageNum;
        page->data = (char*)(bm->mgmtData->poolMem_ptr + frameNum);
        pinRplcStrat(bm, frameNum);
        return RC_OK;
    }

    //Arrive here if the page was not already pinned in a frame
    //Find a frame to put the page in

    BM_Frame *framePtr = findEmptyFrame(bm);
    //if framePtr is NULL, there are no frames available
    if(!framePtr)
        return RC_BM_NO_FRAME_AVAIL;

    //Arrive here if we have a valid framePtr to a frame
    //But we need to forcePage to disk first IF DIRTY
    frameNum = ((framePtr - bm->mgmtData->poolMem_ptr));
    if(bm->mgmtData->isDirtyArray[frameNum] == true) {
        BM_PageHandle *ph = (BM_PageHandle*) calloc(1, sizeof(BM_PageHandle));
        ph->pageNum = bm->mgmtData->frameContent[frameNum];
        ph->data = (char*)framePtr;
        forcePage(bm, ph);
        free(ph);
    }

    //Maybe try to read from disk
    struct SM_FileHandle fHandle;
    RC returnCode;
    if((returnCode = openPageFile(bm->pageFile,&fHandle))!=RC_OK) {
        return returnCode;
    }
    if(fHandle.totalNumPages>pageNum) {
        if((returnCode = readBlock(pageNum,&fHandle,((SM_PageHandle)framePtr)))!=RC_OK)
            return returnCode;
        bm->mgmtData->numReadIO++;
    }
    if((returnCode=closePageFile(&fHandle))!=RC_OK)
        return returnCode;

    page->pageNum = pageNum;
    page->data = (char*)framePtr;
    //Calculate frameNum from framePtr to increment pool info

    bm->mgmtData->fixCountArray[frameNum] = 1;
    bm->mgmtData->frameContent[frameNum] = pageNum;

    pinRplcStrat(bm, frameNum);

    return RC_OK;
}

static void pinRplcStrat(BM_BufferPool* bm, int frameNum) {
    switch(bm->strategy) {
    case RS_FIFO:
        fifoPin(bm, frameNum);
        break;
    case RS_LRU:
        lruPin(bm,frameNum);
        break;
    case RS_CLOCK:
        clockPin(bm,frameNum);
        break;
    case RS_LFU:
        lfuPin(bm, frameNum);
        break;
    case RS_LRU_K:
        break;
    }
}

static BM_Frame * findEmptyFrame(BM_BufferPool *bm) {
    //validate input
    if(!bm)
        return NULL;

    //search for empty frame
    for(int i = 0; i < bm->numPages; i++) {
        if(bm->mgmtData->frameContent[i] == NO_PAGE) {
            return (bm->mgmtData->poolMem_ptr + i);
        }
    }

    //if empty frame not found, search for replacement frame

    switch(bm->strategy) {
    case RS_FIFO:
        return fifoReplace(bm);
        break;
    case RS_LRU:
        return lruReplace(bm);
        break;
    case RS_CLOCK:
        return clockReplace(bm);
        break;
    case RS_LFU:
        return lfuReplace(bm);
        break;
    case RS_LRU_K:
        break;
    default:
        break;
    }

    return NULL;
}


/*********************************************************************
unpinPage unpins the page page. The pageNum field of page should be
used to figure out which page to unpin.
*********************************************************************/
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page) {
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    if(!page)
        return RC_BM_PAGE_NOT_FOUND;

    int frameNum = findFrameNumber(bm, page->pageNum);
    if(bm->mgmtData->fixCountArray[frameNum] > 0)
        bm->mgmtData->fixCountArray[frameNum] -= 1;

    return RC_OK;
}

/*********************************************************************
markDirty marks a page as dirty.
*********************************************************************/
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page) {
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    if(!page)
        return RC_BM_PAGE_NOT_FOUND;

    //initialize variables
    int frameNum = 0;

    //search through the pages stored in the buffer pool for the page of interest
    if((frameNum = findFrameNumber(bm, page->pageNum)) == NO_PAGE)
        return RC_BM_PAGE_NOT_FOUND;
    bm->mgmtData->isDirtyArray[frameNum] = true;

    return RC_OK;
}

/*********************************************************************
forcePage should write the current content of the page back to the
page file on disk.
*********************************************************************/
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page) {
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    if(!page)
        return RC_BM_PAGE_NOT_FOUND;

    SM_FileHandle *fHandle = (SM_FileHandle*) calloc(1, sizeof(SM_FileHandle));
    if(!fHandle) {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    RC returnCode = RC_INIT;

    if((returnCode = openPageFile(bm->pageFile, fHandle)) != RC_OK) {
        free(fHandle);
        fHandle = NULL;
        return returnCode;
    }
    if((returnCode = writeBlock(page->pageNum, fHandle, (char*)page->data)) != RC_OK) {
        free(fHandle);
        fHandle = NULL;
        return returnCode;
    }
    if((returnCode = closePageFile(fHandle)) != RC_OK) {
        free(fHandle);
        fHandle = NULL;
        return returnCode;
    }
    bm->mgmtData->numWriteIO++;
    //search through the pages stored in the buffer pool for the page of interest
    int frameNum = -1;
    if((frameNum = findFrameNumber(bm, page->pageNum)) == -1)
        return RC_BM_PAGE_NOT_FOUND;
    bm->mgmtData->isDirtyArray[frameNum] = false;

    free(fHandle);
    fHandle = NULL;
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
PageNumber *getFrameContents (BM_BufferPool *const bm) {
    return (PageNumber*) bm->mgmtData->frameContent;
}

/*********************************************************************
getDirtyFlags returns an array of bools (of size
numPages) where the ith element is TRUE if the page stored in the ith
page frame is dirty. Empty page frames are considered as clean.
*********************************************************************/
bool *getDirtyFlags (BM_BufferPool *const bm) {
    return bm->mgmtData->isDirtyArray;
}
/*********************************************************************
getFixCounts returns an array of ints (of size numPages)
where the ith element is the fix count of the page stored in the ith
page frame. Return 0 for empty page frames.
*********************************************************************/
int *getFixCounts (BM_BufferPool *const bm) {
    return bm->mgmtData->fixCountArray;
}

/*********************************************************************
getNumReadIO returns the number of pages that have been
read from disk since a buffer pool has been initialized. You code is
responsible to initializing this statistic at pool creating time and
update whenever a page is read from the page file into a page frame.
*********************************************************************/
int getNumReadIO (BM_BufferPool *const bm) {
    return bm->mgmtData->numReadIO;
}

/*********************************************************************
getNumWriteIO returns the number of pages written to the page file
since the buffer pool has been initialized.
*********************************************************************/
int getNumWriteIO (BM_BufferPool *const bm) {
    return bm->mgmtData->numWriteIO;
}

/*********************************************************************
*
*                        HELPER FUNCTIONS
*
*********************************************************************/

/*********************************************************************
Helper function to find the frame number for given pageNumber in
a buffer pool. If a page number doesn't exist then the function will
return NO_PAGE = -1.
*********************************************************************/
static int findFrameNumber(BM_BufferPool * bm, PageNumber pageNumber) {
    //get memory address frameContentArray
    int *frameContent = bm->mgmtData->frameContent;

    //search through the pages stored in the buffer pool
    //for the page of interest
    for (int i = 0; i < bm->numPages; i++) {
        if (frameContent[i] == pageNumber)
            return i;
    }
    return NO_PAGE;
}
