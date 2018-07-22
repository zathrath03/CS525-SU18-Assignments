#include <stdbool.h>
#include "buffer_mgr.h"
#include "replace_strat.h"

BM_PageHandle * fifoReplace(BM_BufferPool *const bm){

    return ((void*)0);
}

BM_PageHandle * lruReplace(BM_BufferPool *const bm){

    return ((void*)0);
}

BM_PageHandle * clockReplace(BM_BufferPool *const bm){
    //get memory address of the first page
    BM_PoolInfo *poolInfo = bm->mgmtData;
    RS_ClockInfo *clockInfo = poolInfo->rplcStratStruct;
    int frameNum = clockInfo->curPage % bm->numPages;

    //search through the pages stored in the buffer pool for the page of interest
    while(true){
        //frame_ptr = poolInfo->poolMem_ptr + frameNum;

        if (poolInfo->fixCountArray[frameNum] == 0 && clockInfo->wasReferencedArray == false)
            return poolInfo->poolMem_ptr + frameNum;

        frameNum = (frameNum + 1) % bm->numPages;
    }

    return ((void*)0);
}

BM_PageHandle * lfuReplace(BM_BufferPool *const bm){

    return ((void*)0);
}
