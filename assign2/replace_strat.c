#include <stdlib.h>
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

/*********************************************************************
*
*                    FIFO Replacement Functions
*
*********************************************************************/
void fifoInit(BM_BufferPool *bm){
}

void fifoFree(BM_BufferPool *const bm){
}

void fifoPin(BM_BufferPool *bm, int frameNum){
}

BM_PageHandle * fifoReplace(BM_BufferPool *const bm){

    return ((void*)0);
}

/*********************************************************************
*
*                     LRU Replacement Functions
*
******************                              *********************
This implementation is based on Modern Operating Systems, 2nd Edition
By Andrew S. Tanenbaum. Check Link Below:
http://www.informit.com/articles/article.aspx?p=25260&seqNum=7
*********************************************************************/
BM_PageHandle * lruReplace(BM_BufferPool *const bm){
    BM_PageHandle* ph = bm->mgmtData->poolMem_ptr;
    int PageNumber = lruFindToReplace(bm);
    ph+=PageNumber;
    return ph;
}

void lruInit(BM_BufferPool * bm){
    int **matrix = (int**) malloc(bm->numPages*(sizeof(int *)));
    for(int i = 0; i<bm->numPages;i++){
        matrix[i]=(int*)malloc((bm->numPages*(sizeof(int*))));
        for(int j = 0; j <bm->numPages;j++){
            matrix[i][j] = 0;
        }
    }
    bm->mgmtData->rplcStratStruct = matrix;
}

void lruPin(BM_BufferPool * bm,int frameNumber){
    int **matrix = (int **) bm->mgmtData->rplcStratStruct;
    //set the row of the frameNumber to 1
    for(int i=0;i<bm->numPages;i++){
        matrix[frameNumber][i]=1;
    }
    //set the column of the frameNumber to 0
    for(int i=0;i<bm->numPages;i++){
        matrix[i][frameNumber]=0;
    }
}

void lruFree(BM_BufferPool * bm){
    int **matrix = (int **) bm->mgmtData->rplcStratStruct;
    //free all row arrays
    for(int c = 0; c<bm->numPages;c++){
        free(matrix[c]);
    }
    //free the main pointer
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

/*********************************************************************
*
*                     Clock Replacement Functions
*
*********************************************************************/
void clockInit(BM_BufferPool *bm){
    //allocate memory for the RS_ClockInfo struct
    RS_ClockInfo *clockInfo = ((RS_ClockInfo *) malloc (sizeof(RS_ClockInfo)));

    //Store a reference to clockInfo in the BufferPool struct
    bm->mgmtData->rplcStratStruct = clockInfo;

    //initialize the attributes of the clockInfo struct
    bool wasReferencedArray[bm->numPages];
    for(int i = 0; i < bm->numPages; i++)
        wasReferencedArray[i] = false;
    clockInfo->wasReferencedArray = wasReferencedArray;
    clockInfo->curPage = 0;
}

void clockFree(BM_BufferPool *const bm){
    RS_ClockInfo *clockInfo = bm->mgmtData->rplcStratStruct;
    free(clockInfo->wasReferencedArray);
    free(clockInfo);
}

void clockPin(BM_BufferPool *const bm, int frameNum)
{
    BM_PoolInfo *poolInfo = bm->mgmtData;
    RS_ClockInfo *clockInfo = poolInfo->rplcStratStruct;
    clockInfo->wasReferencedArray[frameNum] = true;
}

BM_PageHandle * clockReplace(BM_BufferPool *const bm){
    BM_PoolInfo *poolInfo = bm->mgmtData;
    RS_ClockInfo *clockInfo = poolInfo->rplcStratStruct;

    //validate that curPage <= numPages
    clockInfo->curPage = clockInfo->curPage % bm->numPages;

    //search through the poolInfo arrays to find a page with fixCount=0 and ref=false
    while(true){
        //returns a pointer to the first frame that has fixCount=0 and ref=false
        if (poolInfo->fixCountArray[clockInfo->curPage] == 0 && clockInfo->wasReferencedArray == false){
            //increment curPage to prevent always replacing the same page
            clockInfo->curPage = (clockInfo->curPage + 1) % bm->numPages;
            //return the frame pointer
            return poolInfo->poolMem_ptr + clockInfo->curPage;
        }

        //reset the ref to false
        clockInfo->wasReferencedArray[clockInfo->curPage] = false;
        //increment to next frame (or first frame if at end)
        clockInfo->curPage = (clockInfo->curPage + 1) % bm->numPages;
    }
}

/*********************************************************************
*
*                     LFU Replacement Functions
*
*********************************************************************/
void lfuInit(BM_BufferPool *bm){
}

void lfuFree(BM_BufferPool *const bm){
}

void lfuPin(BM_BufferPool *const bm, int frameNum){
}

BM_PageHandle * lfuReplace(BM_BufferPool *const bm){

    return ((void*)0);
}
