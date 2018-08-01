#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "record_mgr.h"
#include "buffer_mgr.h"




// Macros
#define VALID_CALLOC(type, varName, number, size)   \
    type *varName = (type *) calloc(number, size);  \
    if(!varName){                                   \
        printError(RC_BM_MEMORY_ALOC_FAIL);         \
        exit(-1);                                   \
    }

#define ASSERT_RC_OK(functionCall)          \
    RC returnCode = functionCall;                 \
    if(returnCode != RC_OK )\
       return returnCode;


// Prototypes for helper functions
int static findFreeSlot(bitmap * bitMap);
void static deleteFromFreeLinkedList(RM_PageFileHeader* pfhr,RM_PageHeader *phr, BM_BufferPool*bm);

/*********************************************************************
* Notes:
* This implementation of the bitMap and double linked list
* isn't circular.
* - The next of the tail points to NO_PAGE.
* - The previous of the head points to N0_PAGE as well
* appending is to the head, deleting could be anywhere
*********************************************************************/
/*********************************************************************
*
*                TABLE AND RECORD MANAGER FUNCTIONS
*
*********************************************************************/

/*********************************************************************
initRecordManager doesn't seem to do anything. test_assign3_1.c just
passes NULL in mgmtData
*********************************************************************/
RC initRecordManager (void *mgmtData){
    return RC_OK;
}

/*********************************************************************
shutdownRecordManager also doesn't seem to need to do anything.
Does not need to free RID or table (freed in test_assign3_1.C)
*********************************************************************/
RC shutdownRecordManager (){
    return RC_OK;
}

/*********************************************************************
createTable creates the underlying page file and stores information
about the schema, free-space, ... and so on in the Table Information
page(s).
INPUT:
    name: valid string file name
    schema: fully initialized schema
*********************************************************************/
RC createTable (char *name, Schema *schema){
    //validate input
    //make sure a page file with that name doesn't already exist
    //create a page file
    //open the page file
    //**write page file header**
        //rm_serializer.c has a serializeSchema() function that
        //converts the schema to a string
    //close the page file
    return RC_OK;
}

/*********************************************************************
openTable initializes the RM_TableData struct
INPUT:
    *rel: pointer to allocated memory of an uninitialized RM_TableData
    *name: valid string file name
*********************************************************************/
RC openTable (RM_TableData *rel, char *name){
    // validate input
    // open the page file
    // initialize a buffer pool
        // ?? How many pages should we use?
    // pin page with pageFile header
    // create a pointer and allocate memory for a schema struct
    // **populate the schema struct with the schema from disk**
    // initialize rel->schema
    // initialize rel->name = name;
    // initialize rel->mgmtData

    return RC_OK;
}

/*********************************************************************
closeTable causes all outstanding changes to the table to be written
to the page file
INPUT: pointer to an initialized RM_TableData struct
*********************************************************************/
RC closeTable (RM_TableData *rel){
    // validate input
    // shutdown the buffer pool (which forces a pool flush)
    // close the page file
    // free BM_BufferPool pointer
        //should we add this back to shutdownBufferPool()?
    // free rel->mgmtData and rel->schema pointers
        //don't free rel->name since it's allocated by the caller
    return RC_OK;
}

/*********************************************************************
deleteTable deletes the underlying page file
INPUT: name of the pageFile where the table is stored
*********************************************************************/
RC deleteTable (char *name){
    // destroyPageFile(name)
    return RC_OK;
}

/*********************************************************************
getNumTuples returns the number of tuples in the table
INPUT: initialized RM_TableData of interest
*********************************************************************/
int getNumTuples (RM_TableData *rel){
    // validate input
    // pin the page with the pageFile header
    // read numTuples from the header
    // return numTuples
    return 0;
}

/*********************************************************************
*
*                        RECORD FUNCTIONS
*
*********************************************************************/

/*********************************************************************
insertRecord locates a free slot in the pageFile and inserts the record
INPUT:
    *rel: initialized RM_TableData to insert the record into
    *record: id is null, *data contains record to insert
*********************************************************************/
RC insertRecord (RM_TableData *rel, Record *record){
    //validate input
    if(!rel)
        return RC_RM_INIT_ERROR;
    if(!record)
        return RC_RM_INIT_ERROR;
    //create two local BM_PageHandles
    BM_PageHandle pageFileHeader;
    BM_PageHandle pageToInsert;
    BM_BufferPool* bm = rel->bufferPool;
    //pin the page with the pageFile header
    ASSERT_RC_OK(pinPage(bm,&pageFileHeader,0));
    RM_PageFileHeader* pfhr = (RM_PageFileHeader *) &pageFileHeader;
    //find the first page with free slot from pageFile header
    int freePageNum = pfhr->nextFreePage;
    if(freePageNum==NO_PAGE)
        return RC_RM_NO_FREE_PAGES;
    //update record->id.page
    record->id.page = freePageNum;
    //pin the first page with a free slot
    ASSERT_RC_OK(pinPage(bm,&pageToInsert,freePageNum));
    //find free slot using pageHeader bitMap
    RM_PageHeader * phr = (RM_PageHeader *) &pageToInsert;
    int nextFreeSlot = findFreeSlot(phr->freeBitMap);
    if(nextFreeSlot==phr->freeBitMap->bits)
        return RC_RM_NO_FREE_PAGES;
    //update record->id.slot
    record->id.slot = nextFreeSlot;
    //read location of next free slot from current slot
    int recordSize = getRecordSize(rel->schema);
    char * slotPtr = (char*) &pageToInsert+sizeof(RM_PageHeader) + (nextFreeSlot * recordSize );
    //write record->data to current slot
    //maybe we don't need to do recordSize +1 because we don't want the null char
    //at the end of the record data array
    memcpy(slotPtr, record->data,recordSize);
    //update the bitMap
    bitmap_set(phr->freeBitMap, nextFreeSlot);
    //check if the page doesn't have anymore slots
    //so the page will be removed from the linked list
    if(findFreeSlot(phr->freeBitMap)==phr->freeBitMap->bits)
    {
        deleteFromFreeLinkedList(pfhr,phr,bm);
    }
    //increment numTuples in the pageFile header
    pfhr->numTuples++;
    //mark pages as dirty
    ASSERT_RC_OK(markDirty(bm, &pageFileHeader));
    ASSERT_RC_OK(markDirty(bm, &pageToInsert));
    //unpin the pageFile header and the page we inserted the record into
    ASSERT_RC_OK(unpinPage(bm,&pageFileHeader));
    ASSERT_RC_OK(unpinPage(bm,&pageToInsert));
    //we shouldn't need to worry about writing it back to disk
        //that will be handled by page replacement
    return RC_OK;
}
/*********************************************************************
deleteRecord deletes the record identified by id from *rel
INPUT:
    *rel: initialized RM_TableData to insert the record into
    id: initialized with the page and slot of the record of interest
*********************************************************************/
RC deleteRecord (RM_TableData *rel, RID id){
    //get location
    int pageNum = id.page;
    short slotNum = id.slot;
    //create two local BM_PageHandle
    //create two local BM_PageHandles
    BM_PageHandle pageFileHeader;
    BM_PageHandle pageToDelete;
    BM_BufferPool* bm = rel->bufferPool;
    //pin the page with the pageFile header
    ASSERT_RC_OK(pinPage(bm,&pageFileHeader,0));
    RM_PageFileHeader* pfhr = (RM_PageFileHeader *) &pageFileHeader;
    ASSERT_RC_OK(pinPage(bm,&pageToDelete,pageNum));
    //find free slot using pageHeader bitMap
    RM_PageHeader * phr = (RM_PageHeader *) &pageToDelete;
    int recordSize = getRecordSize(rel->schema);
    //delete the record at id.slot
        //(offset by slot*recordSize+sizeof(short))
    char * slotPtr = (char*) &pageToDelete+sizeof(RM_PageHeader) + (slotNum * recordSize );
    //update bitMap
    bitmap_clear(phr->freeBitMap, slotNum);
    //Not sure if this is truly necessary
    //if we update the bitMap then we won't read from that slot anymore
    //this is just for safety and can be taken out
    char emptyRecord[recordSize] = {NULL};
    memcpy(slotPtr, emptyRecord, recordSize);

    //check if the page doesn't have anymore slots
    //so the page will be removed from the linked list
    if(findFreeSlot(phr->freeBitMap)==phr->freeBitMap->bits)
    {
        deleteFromFreeLinkedList(pfhr,phr,bm);
    }
    //read free space ptr from page's header
    //write free space ptr to newly freed slot
    //update page's free space ptr to point to newly freed slot
        //check if it previously had a free slot for later use
    //decrement numTuples
    pfhr->numTuples--;
    //mark pages as dirty
    ASSERT_RC_OK(markDirty(bm, &pageFileHeader));
    ASSERT_RC_OK(markDirty(bm, &pageToDelete));
    //unpin the pageFile header and the page we inserted the record into
    ASSERT_RC_OK(unpinPage(bm,&pageFileHeader));
    ASSERT_RC_OK(unpinPage(bm,&pageToDelete));
    //we shouldn't need to worry about writing it back to disk
        //that will be handled by page replacement
    //**if the page didn't previously have a free slot, check
        //to see if it is now the first page with a free slot
        //and update pageFile header appropriately
    return RC_OK;
}

/*********************************************************************
updateRecord replaces the data in a slot with the data in *record
INPUT:
    *rel: initialized RM_TableData to update the record of
    *record: id contains page and slot, *data contains record to insert
*********************************************************************/
RC updateRecord (RM_TableData *rel, Record *record){
    //create two local BM_PageHandle
    //pin pageFile header
    //find the size of a record(slot) from pageFile header
    //pin page given by record->id.page
    //write record->data to the slot identified by record->id.slot
    //unpin pages
    return RC_OK;
}

/*********************************************************************
getRecord retrieves the data id.page and id.slot and initializes *record
INPUT:
    *rel: initialized RM_TableData to retrieve the record from
    id: contains the page and slot of the record of interest
    *record: allocated, but uninitiated Record to store data in
*********************************************************************/
RC getRecord (RM_TableData *rel, RID id, Record *record){
    //create two local BM_PageHandle
    //pin pageFile header
    //find the size of a record(slot) from pageFile header
    //pin page given by id.page
    //read data from slot given by id.slot
        //(offset by slot*recordSize+sizeof(short))
    //write data to record->data
    //store id in record->id
    //unpin pages
    return RC_OK;
}

/*********************************************************************
*
*                        SCAN FUNCTIONS
*
*********************************************************************/
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond){
    return RC_OK;
}
RC next (RM_ScanHandle *scan, Record *record){
    return RC_OK;
}
RC closeScan (RM_ScanHandle *scan){
    return RC_OK;
}

/*********************************************************************
*
*                        SCHEMA FUNCTIONS
*
*********************************************************************/
int getRecordSize (Schema *schema){
    return 0;
}
Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys){
    VALID_CALLOC(Schema, schema, 1, sizeof(Schema));
    return schema;
}
RC freeSchema (Schema *schema){
    return RC_OK;
}

/*********************************************************************
*
*                        ATTRIBUTE FUNCTIONS
*
*********************************************************************/
RC createRecord (Record **record, Schema *schema){
    return RC_OK;
}
RC freeRecord (Record *record){
    return RC_OK;
}
RC getAttr (Record *record, Schema *schema, int attrNum, Value **value){
    return RC_OK;
}
RC setAttr (Record *record, Schema *schema, int attrNum, Value *value){
    return RC_OK;
}

/*********************************************************************
*
*                        HELPER FUNCTIONS
*
*********************************************************************/


/*****************************************
checks if the bitMap has a freeslot
if not it return bitMap size as the next
free slot index
*****************************************/
int static findFreeSlot(bitmap * bitMap){
    int mapSize = bitMap->bits;
    for(int i=0; i<mapSize;i++)
    {
        if(bitmap_read(bitMap,i)==0)
        {
            return i;
        }
    }
    return mapSize;
}

void static deleteFromFreeLinkedList(RM_PageFileHeader* pfhr,
                                     RM_PageHeader *phr, BM_BufferPool*bm){
    //update nextPage, prevPage
    BM_PageHandle nextPageHandle;
    BM_PageHandle prevPageHandle;
    //this page is the header
    if(phr->prevFreePage == NO_PAGE){
        //update pageFileHeader
        pfhr->nextFreePage = phr->nextFreePage;
    }
    else{
        //update next page Header
        ASSERT_RC_OK(pinPage(bm, &prevPageHandle, phr->prevFreePage));
        RM_PageHeader * pphr = (RM_PageHeader *) &prevPageHandle;
        //update the head
        pphr->nextFreePage = phr->nextFreePage;
        ASSERT_RC_OK(markDirty(bm,&prevPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&prevPageHandle));
    }
    //if the page is not the tail
    if(phr->nextFreePage != NO_PAGE)
    {
        //update next page Header
        ASSERT_RC_OK(pinPage(bm, &nextPageHandle, phr->nextFreePage));
        RM_PageHeader * nphr = (RM_PageHeader *) &nextPageHandle;
        //update the head
        nphr->prevFreePage = phr->prevFreePage;
        ASSERT_RC_OK(markDirty(bm,&nextPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&nextPageHandle));
    }
    //update the current page next and prev
    phr->nextFreePage = NO_PAGE;
    phr->prevFreePage = NO_PAGE;
}
