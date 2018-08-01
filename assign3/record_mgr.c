#include <stdlib.h>

#include "record_mgr.h"
#include "buffer_mgr.h"

// Data structures

// Prototypes for helper functions

// Macros
#define VALID_CALLOC(type, varName, number, size)   \
    type *varName = (type *) calloc(number, size);  \
    if(!varName){                                   \
        printError(RC_BM_MEMORY_ALOC_FAIL);         \
        exit(-1);                                   \
    }

#define ASSERT_RC_OK(functionCall)          \
    RC returnCode = RC_INIT;                \
    if((returnCode = functionCall) != RC_OK \
       return returnCode;

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
    //create two local BM_PageHandles
    //pin the page with the pageFile header
    //find the first page with free slot from pageFile header
    //pin the first page with a free slot
    //find free slot using free space offset in page header
    //read location of next free slot from current slot
        //if there isn't another free slot in this page, we need to
        //find the next page that has a free slot and update the
        //pageFile header. A free slot must not protrude past the end of the page
    //write location of next free slot to page header
    //write record->data to current slot
    //increment numTuples in the pageFile header
    //unpin the pageFile header and the page we inserted the record into
    //update record->id.page and record->id.slot
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
    //create two local BM_PageHandle
    //pin pageFile header
    //find the size of a record(slot) from pageFile header
    //pin page given by id.page
    //delete the record at id.slot
        //(offset by slot*recordSize+sizeof(short))
    //read free space ptr from page's header
    //write free space ptr to newly freed slot
    //update page's free space ptr to point to newly freed slot
        //check if it previously had a free slot for later use
    //decrement numTuples
    //**if the page didn't previously have a free slot, check
        //to see if it is now the first page with a free slot
        //and update pageFile header appropriately
    //unpin pages
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
