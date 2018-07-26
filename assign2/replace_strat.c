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
	RS_FIFOInfo *fifoInfo = ((RS_FIFOInfo *) malloc (sizeof(RS_FIFOInfo)));

	if (fifoInfo == NULL){
		printError(RC_BM_MEMORY_ALOC_FAIL);
		exit(-1);
	}

	fifoInfo->head = NULL;
	fifoInfo->tail = NULL;

	bm->mgmtData->rplcStratStruct = fifoInfo;

}

void fifoFree(BM_BufferPool *const bm){
	RS_FIFOInfo *fifoInfo = bm->mgmtData->rplcStratStruct;
	listNode *curr = fifoInfo->head;
	while(curr != NULL){
		curr = curr->nextNode;
		free(curr);
	}
	free(curr);
	free(fifoInfo);
}

void fifoPin(BM_BufferPool *bm, int frameNum){
	BM_PoolInfo *poolInfo = bm->mgmtData;
	RS_FIFOInfo *fifoInfo = poolInfo->rplcStratStruct;

	listNode *node = ((listNode *) malloc (sizeof(listNode)));
	node->nextNode = NULL;
	if(fifoInfo->head == NULL){
		node->nextNode = NULL;
		fifoInfo->head = node;
		fifoInfo->tail = node;
	}
	else{
		node->nextNode = NULL;
		fifoInfo->tail = node;
	}
}

BM_PageHandle * fifoReplace(BM_BufferPool *const bm){
	BM_PoolInfo *poolInfo = bm->mgmtData;
	//BM_PageHandle* ph = poolInfo->poolMem_ptr;
	RS_FIFOInfo *fifoInfo = poolInfo->rplcStratStruct;

	listNode *prev = fifoInfo->head;
	listNode *curr = prev->nextNode;
	if(curr == NULL){
		int frameNum = prev->frameNum;
		if(bm->mgmtData->fixCountArray[frameNum] == 0){
			return bm->mgmtData->poolMem_ptr + frameNum;
		}
	}
	else{
		while(curr != NULL){

			int frameNum = curr->frameNum;
			if(bm->mgmtData->fixCountArray[frameNum] == 0){
				prev->nextNode = curr->nextNode;
				return bm->mgmtData->poolMem_ptr + frameNum;
			}

			prev = curr;
			curr = curr->nextNode;

		}
	}
    return NULL;
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
    if(PageNumber==NO_PAGE)
        return NULL;
    ph+=PageNumber;
    return ph;
}

void lruInit(BM_BufferPool * bm){
    int **matrix = (int**) malloc(bm->numPages*(sizeof(int *)));
    if(!matrix)
    {
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }

    for(int i = 0; i<bm->numPages;i++){
        matrix[i]=(int*)calloc(bm->numPages,sizeof(int));
        if(!matrix[i])
        {
            printError(RC_BM_MEMORY_ALOC_FAIL);
            exit(-1);
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
        matrix[c]=NULL;
    }
    //free the main pointer
    free(matrix);
    matrix = NULL;
}

//private function don't need to implement
int lruFindToReplace(BM_BufferPool *const bm){
    int **matrix = (int **) bm->mgmtData->rplcStratStruct;
    int minIndex = -1;
    int minVal = -1;
    for(int i=0;i<bm->numPages;i++)
    {
        //Skip rows that are pinned by users
        //i.e don't have fixCount ==0
        if( bm->mgmtData->fixCountArray[i]!=0)
            continue;
        int rowMin = bm->numPages;
        for(int j=0;j<bm->numPages;j++)
        {
            if(matrix[i][j]==1)
            {
               rowMin = j;
               break;
            }
        }
        if((rowMin<minVal || minVal ==-1))
        {
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
    //verify valid memory allocation
    if(clockInfo == NULL){
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    //allocate memory for the bool array and initialize to false
    clockInfo->wasReferencedArray = ((bool *) calloc (bm->numPages, sizeof(bool)));
    //verify valid memory allocation
    if(clockInfo->wasReferencedArray == NULL){
        printError(RC_BM_MEMORY_ALOC_FAIL);
        exit(-1);
    }
    //Store a reference to clockInfo in the BufferPool struct
    bm->mgmtData->rplcStratStruct = clockInfo;
    //initialize the attributes of the clockInfo struct
    clockInfo->curFrame = 0;
}

void clockFree(BM_BufferPool *const bm){
    RS_ClockInfo *clockInfo = bm->mgmtData->rplcStratStruct;
    free(clockInfo->wasReferencedArray);
    clockInfo->wasReferencedArray = NULL;
    free(clockInfo);
    bm->mgmtData->rplcStratStruct = NULL;
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

    //validate that curFrame <= numPages
    clockInfo->curFrame = clockInfo->curFrame % bm->numPages;

    //search through the poolInfo arrays to find a page with fixCount=0 and ref=false
    while(true){
        //returns a pointer to the first frame that has fixCount=0 and ref=false
        if (poolInfo->fixCountArray[clockInfo->curFrame] == 0 && clockInfo->wasReferencedArray == false){
            //increment curFrame to prevent always replacing the same page
            clockInfo->curFrame = (clockInfo->curFrame + 1) % bm->numPages;
            //return the frame pointer
            return poolInfo->poolMem_ptr + clockInfo->curFrame;
        }

        //reset the ref to false
        clockInfo->wasReferencedArray[clockInfo->curFrame] = false;
        //increment to next frame (or first frame if at end)
        clockInfo->curFrame = (clockInfo->curFrame + 1) % bm->numPages;
    }
}

/*********************************************************************
*
*                     LFU Replacement Functions
*
*********************************************************************/
void lfuInit(BM_BufferPool *bm){
    BM_PoolInfo *poolInfo = bm->mgmtData;
    //Memory allocation needed for the lfuInfo struct
    RS_LFUInfo *lfuInfo = ((RS_LFUInfo *) malloc (sizeof(RS_LFUInfo)));
    //Store a reference to lfuInfo in the BufferPool struct
    poolInfo->rplcStratStruct = lfuInfo;
    //Initialize head and tail to point to NULL
    lfuInfo->head = NULL;
    lfuInfo->tail = NULL;
}

void lfuFree(BM_BufferPool *const bm){
    //free up structure itself
    RS_LFUInfo *lfuInfo = bm->mgmtData->rplcStratStruct;
  	//free every node in the list

    LFUnode *node = lfuInfo->head;
  	LFUnode *temp = node->nextNode;
  	while(temp != NULL){
      free(node); //Free the memory allocated by node
      node = temp;
      temp = temp->nextNode; //temp is now iterated to the next position in the linked list
    } //Breaks out when temp is null
  	free(node); //frees last node in linked list
  	node = NULL;
  	//free structure pointers
    free(lfuInfo->head);
  	lfuInfo->head = NULL;
    free(lfuInfo->tail);
  	lfuInfo->tail = NULL;
  	free(lfuInfo);
  	lfuInfo = NULL;
}

void lfuPin(BM_BufferPool *const bm, int frameNum){
    BM_PoolInfo *poolInfo = bm->mgmtData;
    RS_LFUInfo *lfuInfo = poolInfo->rplcStratStruct;
    //Check if the page already exists to be safe
    LFUnode *node = lfuInfo->head;
    do{
      if (node->frameNumber == frameNum){
        //Increment that node's frequency
        node->frequency++;
        return;
      }
      else
        node = node->nextNode;
    }while(node->nextNode != NULL); //could also use while node != tail

  	//Only arrives here if the page isn't already in the list
    //If new node is entered then do the following
    LFUnode *newNode = (LFUnode*)malloc(sizeof(LFUnode));
    //newNode created and populated
    newNode->frequency = 1;
    newNode->frameNumber = frameNum;
    //ordering it into linked list; both head and tail point to addr of newNode
    if(lfuInfo->head == NULL){
      lfuInfo->head = newNode;
      lfuInfo->tail = newNode;
    }
    else{//setting tail pointer to become new newNode
      (lfuInfo->tail)->nextNode = newNode;
      lfuInfo->tail = newNode;
    }
}

BM_PageHandle * lfuReplace(BM_BufferPool *const bm){
    BM_PoolInfo *poolInfo = bm->mgmtData;
    RS_LFUInfo *lfuInfo = poolInfo->rplcStratStruct;
    BM_PageHandle* ph = bm->mgmtData->poolMem_ptr;
    //Iterate through the linked list to find the smallest frequency value
    LFUnode *node = lfuInfo->head; //used as node we want to return
  	int frequency = 2147483647; //INT_MAX
  	int frameNum = -1;
    while(node != NULL){ //Compare the nodes frequency with the head and find the smallest frequency
      if (node->frequency < frequency && poolInfo->fixCountArray[node->frameNumber] == 0){
        frameNum = node->frameNumber;
        frequency = node->frequency;
      }
      node = node->nextNode;
    }

  	if (frameNum == -1) //there are no pinned frames with fixCount of 0
      return ((void*)0);

    //Now that we have the frame number for the correct node, we will use fifo to find the exact frame we want.
    //If the head node is what we want:
    node = lfuInfo->head;
    if(node->frameNumber == frameNum){ //the head node is the one that needs to be removed
      lfuInfo->head = node->nextNode; //moves the head pointer to the second node
      free(node); //frees the memory allocated to the node we're replacing
      node = NULL;
      return ph + frameNum; //returns a pageHandle of the frame we're offering for replacement
    }
    //If head node wasn't the right frameNumber
    //Iterate through the linked list till we find the correct frame.
    while(node->nextNode->nextNode != NULL){
      if(node->nextNode->frameNumber == frameNum){ //if correct frame is found
        LFUnode *temp = node->nextNode;
        node->nextNode = node->nextNode->nextNode;	//move the next nextnode to take place of the node we're removing
        free(temp);	//Free node we want to remove
        temp=NULL;
        return ph + frameNum;
      }
      else
        node = node->nextNode;	//If the correct node isn't found, then move to the next node and compare
    }
  	//We reach here only if neither the head nor the internal nodes were the right frame number
    //If the tail is the frame we're looking for
  	//node is pointing to the node just before the tail at this point
    if(node->nextNode->frameNumber == frameNum){
      LFUnode *temp = node->nextNode; //save a reference to the node we're removing for later use
      lfuInfo->tail = node; //move the tail pointer to the node before last
      node->nextNode = NULL; //remove the reference to the last node
      free(temp); //free memory held by the last node
      temp = NULL; //for safety to ensure that temp is no longer pointing at deallocated memory
      return ph + frameNum; //returns a pageHandle of the frame we're offering for replacement
    }

  	printf("\nERROR: frame %d not found for removal from linked list in LFUReplace()\n", frameNum);
    return ph + frameNum;
}
