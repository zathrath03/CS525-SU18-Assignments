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
BM_PageHandle * findEmptyFrame(BM_BufferPool *bm);

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

    //Set the isDirtyArray false
    bool * isDirtyArray = (bool *)malloc(sizeof(bool)*bm->numPages);
    for(int i =0; i <bm->numPages;i++){
        isDirtyArray[i]=false;
    }
    pi->isDirtyArray = isDirtyArray;

    //Set the fixCountArray to 0
    int * fixCountArray = (int *)malloc(sizeof(int)*bm->numPages);
    for(int i; i<bm->numPages;i++){
        fixCountArray[i]=0;
    }
    pi->fixCountArray = fixCountArray;

    //Set the frameContent to NO_PAGE
    int * frameContent = (int *)malloc(sizeof(int)*bm->numPages);
    for(int i; i<bm->numPages;i++){
        frameContent[i]=NO_PAGE;
    }
    pi->frameContent = frameContent;

    //allocate memory for pageFrames
    pi->poolMem_ptr = (BM_PageHandle *)malloc(sizeof(BM_PageHandle)*bm->numPages);
    bm->mgmtData = pi;
    return initRelpacementStrategy(bm, strategy, stratData);
}


RC initRelpacementStrategy(BM_BufferPool * bm,ReplacementStrategy strategy,void *stratData){
    switch(strategy){
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
RC shutdownBufferPool(BM_BufferPool *const bm)
{
    RC rc;
    if((rc = forceFlushPool(bm))!=RC_OK )
    {
        return rc;
    }
    //free up space from pageFrames
    BM_PoolInfo *poolInfo = bm->mgmtData;
    //free up pool info
    free(poolInfo->poolMem_ptr);
    free(poolInfo->isDirtyArray);
    free(poolInfo->fixCountArray);
    free(poolInfo->frameContent);
    free(poolInfo);
    //free up replacement Strategy
    if((rc = freeReplacementStrategy(bm))!=RC_OK){
        return rc;
    }
    //free up buffer manager
    free(bm);
    return rc;
}


/********************************************************************
Free up any space allocated for a replacement strategy
*********************************************************************/
RC freeReplacementStrategy(BM_BufferPool *const bm){
    ReplacementStrategy strategy = bm->strategy;
    switch(strategy){
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
RC forceFlushPool(BM_BufferPool *const bm){
    //Utilize for creating and writing to disk
    SM_FileHandle fHandle;
    RC returnCode =  RC_OK;
    //get memory address of the first page
    BM_PoolInfo *poolInfo = bm->mgmtData;
    BM_PageHandle *frame_ptr = poolInfo->poolMem_ptr;
    //openPageFile called
    if((returnCode = openPageFile(bm->pageFile, &fHandle)) != RC_OK)
        return returnCode;

    //iterate through the pages stored in the buffer pool for the page of interest
    for (int i = 0; i < bm->numPages; i++){
        if ((poolInfo->fixCountArray[i] == 0) && (poolInfo->isDirtyArray[i]==true)){
            //once the page with dirty bit true and fix count=0 is found,
            //write block of data to the page file on disk
            if((returnCode = writeBlock((frame_ptr+i)->pageNum, &fHandle, (frame_ptr+i)->data)) != RC_OK)
              return returnCode;
            (poolInfo->isDirtyArray[i]) = false;
            (poolInfo->numWriteIO)++;
        }
    }
    returnCode = closePageFile(&fHandle);
    return returnCode;
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
    PageNumber pageNum){

    int frameNum = findFrameNumber(bm,pageNum);
	BM_PoolInfo *poolInfo = bm->mgmtData;
	BM_PageHandle *framePtr = poolInfo->poolMem_ptr + frameNum;

    if(frameNum == -1){
    	framePtr = findEmptyFrame(bm);

    	page->pageNum = pageNum;
    	frameNum = (framePtr - poolInfo->poolMem_ptr)/sizeof(BM_PageHandle);

    	if(bm->mgmtData->isDirtyArray[frameNum] == true){
    		forcePage(bm, framePtr);
		}

    	SM_FileHandle fHandle;
    	RC returnCode;

    	//openPageFile called
    	if((returnCode = openPageFile(bm->pageFile, &fHandle)) != RC_OK)
    		return returnCode;

    	if((returnCode = readBlock(page->pageNum, &fHandle, page->data)) != RC_OK)
    		return returnCode;

    	if((returnCode = closePageFile(&fHandle)) != RC_OK)
    		return returnCode;
    	bm->mgmtData->fixCountArray[frameNum] = 0;

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
	bm->mgmtData->frameContent[frameNum] = pageNum;
	bm->mgmtData->fixCountArray[frameNum] += 1;


    return RC_OK;
}

BM_PageHandle * findEmptyFrame(BM_BufferPool *bm){
	BM_PageHandle *framePtr = ((void*)0);
	//search for empty frame
	for(int i = 0; i < bm->numPages; i++){
		if(bm->mgmtData->frameContent[i] == -1){
			return bm->mgmtData->poolMem_ptr + i;
		}
	}

	//if empty frame not found, search for replacement frame

	switch(bm->strategy) {
	case RS_FIFO:
		framePtr = fifoReplace(bm);
		break;
	case RS_LRU:
		framePtr = lruReplace(bm);
		break;
	case RS_CLOCK:
		framePtr = clockReplace(bm);
		break;
	case RS_LFU:
		framePtr = lfuReplace(bm);
		break;
	case RS_LRU_K:
		break;
	default:
		break;
	}

	return framePtr;
}


/*********************************************************************
unpinPage unpins the page page. The pageNum field of page should be
used to figure out which page to unpin.
*********************************************************************/
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){

	int frameNum = findFrameNumber(bm, page->pageNum);
	bm->mgmtData->fixCountArray[frameNum] -= 1;
	bm->mgmtData->isDirtyArray[frameNum] = 0;
	bm->mgmtData->frameContent[frameNum] = 1;
	bm->mgmtData->numReadIO += 1;

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
    //validate input
    if(!bm)
        return RC_BM_NOT_ALLOCATED;
    if(!page)
        return RC_BM_PAGE_NOT_FOUND;

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
    return (PageNumber*) bm->mgmtData->frameContent;
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
a buffer pool. If a page number doesn't exist then the function will
return NO_PAGE = -1.
*********************************************************************/
int findFrameNumber(BM_BufferPool * bm, PageNumber pageNumber){
    //get memory address frameContentArray
    int *frameContent = bm->mgmtData->frameContent;

    //search through the pages stored in the buffer pool
    //for the page of interest
    for (int i = 0; i < bm->numPages; i++){
        if (frameContent[i] == pageNumber)
            return i;
    }
    return NO_PAGE;
}
