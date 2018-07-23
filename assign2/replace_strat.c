#include <stdbool.h>
#include "buffer_mgr.h"
#include "replace_strat.h"




/*********************************************************************
*
*                   Replacement functions
*
Your code should implement the following functions:
let repName = your replacement policy name
You should have the following functions implemented
-repNamePin()
-repNameInit()
-repNameFree()
-repNameReplace()

See LRU below as example
*********************************************************************/



BM_PageHandle * fifoReplace(BM_BufferPool *const bm){

    return ((void*)0);
}


void clockPin(BM_BufferPool *const bm, PageNumber frameNum)
{

    BM_PoolInfo *poolInfo = bm->mgmtData;
    RS_ClockInfo *clockInfo = poolInfo->rplcStratStruct;
    clockInfo->wasReferencedArray[frameNum] = true;

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
/*********************************************************************
*
*             LRU Replacement Functions
*
*********************************************************************/
BM_PageHandle * lruReplace(BM_BufferPool *const bm){
    BM_PageHandle* ph = bm->mgmtData->poolMem_ptr;
    int PageNumber = lruFindToReplace(bm);
    ph+=PageNumber;
    return ph;
}

void lruInit(BM_BufferPool * bm){
    int **matrix = (int**) malloc(bm->numPages*(sizeof(int *)));
    for(int c = 0; c<bm->numPages;c++){
        matrix[c]=(int*)malloc((bm->numPages*(sizeof(int*))));
    }
    for(int i = 0; i <bm->numPages;i++){
        for(int j = 0; j <bm->numPages;j++){
            matrix[i][j] = 0;
        }
    }
    bm->mgmtData->rplcStratStruct = matrix;
}

void lruPin(BM_BufferPool * bm,int frameNumber){
    int **matrix = (int **) bm->mgmtData->rplcStratStruct;
    //set the row to 1
    for(int i=0;i<bm->numPages;i++){
        matrix[frameNumber][i]=1;
    }
    //set the column to 0
    for(int i=0;i<bm->numPages;i++){
        matrix[i][frameNumber]=0;
    }
}

void lruFree(BM_BufferPool * bm){
    int **matrix = (int **) bm->mgmtData->rplcStratStruct;
    free(matrix);
}

//private function don't need to implement
int lruFindToReplace(BM_BufferPool *const bm){
    int **matrix = (int **) bm->mgmtData->rplcStratStruct;
    int minIndex;
    int minVal = -1;
    for(int i=0;i<bm->numPages;i++)
        {
        int rowMin = bm->numPages;
        for(int j=0;j<bm->numPages;j++)
        {
            if(matrix[i][j]==1){
               rowMin = j;
               break;
            }
        }
        if(rowMin<minVal || minVal ==-1){
            minVal = rowMin;
            minIndex = i;
        }
    }
    return minIndex;
}
