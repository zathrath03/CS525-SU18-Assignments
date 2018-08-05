#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <io.h>

#include "bitmap.h"
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"

/*********************************************************************
*
*                             MACROS
*
*********************************************************************/
#define VALID_CALLOC(type, varName, number, size)   \
    type *varName = (type *) calloc(number, size);  \
    if(!varName){                                   \
        printError(RC_BM_MEMORY_ALOC_FAIL);         \
        exit(-1);                                   \
    }

/**Must declare RC returnCode in function before using ASSERT_RC_OK**/
#define ASSERT_RC_OK(functionCall)  \
    returnCode = functionCall;      \
    if(returnCode != RC_OK )        \
       return returnCode;

/*********************************************************************
Offset Macros for retrieving data from the PageFile header
*********************************************************************/
#define recordSizeOffset 0
#define numTuplesOffset sizeof(unsigned short)
#define pageNumOffset sizeof(unsigned int)
#define nextFreePageOffset numTuplesOffset + sizeof(unsigned int)
#define numSlotsPerPageOffset nextFreePageOffset + pageNumOffset
#define schemaSizeOffset numSlotsPerPageOffset + sizeof(unsigned short)
#define schemaOffset schemaSizeOffset + sizeof(unsigned short)
#define numAttrOffset schemaOffset
#define dataTypeOffset(i) schemaOffset + ((2*i)+1)*sizeof(unsigned short)
#define typeLengthOffset(i) schemaOffset + 2*(i+1)*sizeof(unsigned short)
/**keySizeOffset requires defining numAttr**/
#define keySizeOffset schemaOffset + ((2*numAttr)+1)*sizeof(unsigned short)
#define keyAttrOffset(i) keySizeOffset + i*sizeof(unsigned short)
/**firstNameOffset requires defining keySize**/
#define attrNamesOffset keySizeOffset + keySize*sizeof(unsigned short)

/*********************************************************************
Offset Macros for retrieving data from the Page header
*********************************************************************/
#define bitmapOffset(i) i*sizeof(bitmap_type) //i is the bitmap->words

/*********************************************************************
*
*                       FUNCTION PROTOTYPES
*
*********************************************************************/
// Prototypes for helper functions
int static findFreeSlot(bitmap * bitMap);
static RC preparePFHdr(Schema *schema, char *pHandle);
static RC deleteFromFreeLinkedList(char* pfhr,char *phr, BM_BufferPool*bm);
static RC appendToFreeLinkedList(char * pfhr, char * phr,BM_BufferPool * bm);
static int getAttrOffset(Schema *schema, int attrNum);
static RC findNewPageNum(RM_TableData * rel, unsigned int * nextFreePage);
static unsigned short calcNumSlotsPerPage(unsigned short recordSize);

// Prototypes for getters and setters for pagefile header data
static unsigned short getRecordSizePF(char *pfHdrFrame);
static unsigned int getNumTuplesPF(char *pfHdrFrame);
static void setNumTuplesPF(char *pfHdrFrame, unsigned int numTuples);
static unsigned int getNextFreePage(char *pfHdrFrame);
static void setNextFreePage(char *pfHdrFrame, unsigned int nextFreePage);
static unsigned short getNumSlotsPerPage(char *pfHdrFrame);
static unsigned short getSchemaSize(char *pfHdrFrame);
static void getSchema(char *pfHdrFrame, Schema *schema);
static unsigned short getNumAttr(char *pfHdrFrame);
static DataType getIthDataType(char *pfHdrFrame, int ithSlot);
static unsigned short getIthTypeLength(char *pfHdrFrame, int ithSlot);
static unsigned short getKeySize(char *pfHdrFrame);
static unsigned short getIthKeyAttr(char *pfHdrFrame, int ithSlot);
static unsigned short getIthNameLength(char *pfHdrFrame, int ithSlot);
static char* getIthAttrName(char *pfHdrFrame, int ithSlot);

//prototypes for getters and setters for page header
static bitmap* getBitMapPH(char * phrFrame);
static int getBitMapWordsPH(char* phrFrame);
static int getBitMapBitsPH(char* phrFrame);
static void setBitMapPH(char * phrFrame, bitmap* b);
static void setBitMapArrayPH(char* phrFrame, bitmap * b);
static unsigned int getPrevFreePagePH(char *phrFrame);
static unsigned int getNextFreePagePH(char *phrFrame);
static void setPrevFreePagePH(char * phrFrame, unsigned int pageNum);
static void setNextFreePagePH(char * phrFrame, unsigned int pageNum);

/*********************************************************************
* Notes:
* This implementation of the bitMap and double linked list
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
RC initRecordManager (void *mgmtData)
{
    return RC_OK;
}

/*********************************************************************
shutdownRecordManager also doesn't seem to need to do anything.
Does not need to free RID or table (freed in test_assign3_1.C)
*********************************************************************/
RC shutdownRecordManager ()
{
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
RC createTable (char *name, Schema *schema)
{
    RC returnCode = RC_INIT;
    //validate input
    if(!name || !schema)
        return RC_RM_INIT_ERROR;
    //make sure a page file with that name doesn't already exist
//TODO: uncomment the file existence check when testing is complete
//    if(!access(name, F_OK))
//        return RC_RM_FILE_ALREADY_EXISTS;
    //create a page file
    ASSERT_RC_OK(createPageFile(name));
    //open the page file
    SM_FileHandle fHandle;
    ASSERT_RC_OK(openPageFile(name, &fHandle));
    //write page file header
    VALID_CALLOC(char, pHandle, 1, PAGE_SIZE);
    ASSERT_RC_OK(preparePFHdr(schema, pHandle));
    ASSERT_RC_OK(writeBlock(0, &fHandle, pHandle));
    //close the page file
    ASSERT_RC_OK(closePageFile(&fHandle));
    //free allocated memory
    free(pHandle);
    pHandle = NULL;
    return RC_OK;
}

/*********************************************************************
openTable initializes the RM_TableData struct
INPUT:
    *rel: pointer to allocated memory of an uninitialized RM_TableData
    *name: valid string file name
*********************************************************************/
RC openTable (RM_TableData *rel, char *name)
{
    RC returnCode = RC_INIT;
    // validate input
    if(!rel || !name)
        return RC_RM_INIT_ERROR;
    // open the page file
    SM_FileHandle fHandle;
    ASSERT_RC_OK(openPageFile(name, &fHandle));
    // initialize a buffer pool
    VALID_CALLOC(BM_BufferPool, bm, 1, sizeof(BM_BufferPool));
    ASSERT_RC_OK(initBufferPool(bm, name, 1000, RS_LRU, NULL));
    // pin page with pageFile header
    BM_PageHandle pfHdr;
    ASSERT_RC_OK(pinPage(bm, &pfHdr, 0));
    // create a pointer and allocate memory for a schema struct
    VALID_CALLOC(Schema, schema, 1, sizeof(Schema));
    // populate the schema struct with the schema from disk
    getSchema(pfHdr.data, schema);
    // initialize RM_TableData
    rel->name = name;
    rel->schema = schema;
    rel->bufferPool = bm;
    // close the page file
    ASSERT_RC_OK(closePageFile(&fHandle));
    // unpin page with pageFile header
    ASSERT_RC_OK(unpinPage(bm, &pfHdr));

    return RC_OK;
}

/*********************************************************************
closeTable causes all outstanding changes to the table to be written
to the page file
INPUT: pointer to an initialized RM_TableData struct
*********************************************************************/
RC closeTable (RM_TableData *rel)
{
    // validate input
    if(!rel)
        return RC_RM_INIT_ERROR;
    // shutdown the buffer pool (which forces a pool flush)
    RC returnCode = RC_INIT;
    ASSERT_RC_OK(shutdownBufferPool(rel->bufferPool));
    // free memory allocated for arrays in schema
    ASSERT_RC_OK(freeSchema(rel->schema));
    // free BM_BufferPool pointer
    free(rel->bufferPool);
    // don't free rel->name since it's allocated by the caller
    return RC_OK;
}

/*********************************************************************
deleteTable deletes the underlying page file
INPUT: name of the pageFile where the table is stored
*********************************************************************/
RC deleteTable (char *name)
{
    // validate input
    if(!name)
        return RC_RM_INIT_ERROR;
    // destroyPageFile(name)
    destroyPageFile(name);
    return RC_OK;
}

/*********************************************************************
getNumTuples returns the number of tuples in the table
INPUT: initialized RM_TableData of interest
*********************************************************************/
int getNumTuples (RM_TableData *rel)
{
    // validate input
    if(!rel)
        return RC_RM_INIT_ERROR;
    // pin the page with the pageFile header
    BM_PageHandle pfHdr;
    RC returnCode = RC_INIT;
    ASSERT_RC_OK(pinPage(rel->bufferPool, &pfHdr, 0));
    // read numTuples from the header
    int numTuples = getNumTuplesPF(pfHdr.data);
    // unpin the page with the pageFile header
    ASSERT_RC_OK(unpinPage(rel->bufferPool, &pfHdr));
    // return numTuples
    return numTuples;
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
RC insertRecord (RM_TableData *rel, Record *record)
{
    RC returnCode = RC_INIT;
    bool newPageCreated = false;

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
    //find the first page with free slot from pageFile header
    unsigned int freePageNum = getNextFreePage(pageFileHeader.data);
    if(freePageNum==0)
    {
        ASSERT_RC_OK(findNewPageNum(rel, &freePageNum));
        newPageCreated=true;
    }
    //update record->id.page
    record->id.page = freePageNum;
    //pin the first page with a free slot
    ASSERT_RC_OK(pinPage(bm,&pageToInsert,freePageNum));
    //setup page header if it is a new page
    if(newPageCreated)
    {
        ASSERT_RC_OK(forcePage(bm, &pageToInsert));
        //set up pages
        setNextFreePagePH(pageToInsert.data, 0);
        setPrevFreePagePH(pageToInsert.data, freePageNum);
        appendToFreeLinkedList(pageFileHeader.data, pageToInsert.data, bm);
        bitmap * b = bitmap_allocate((int) getNumSlotsPerPage(pageFileHeader.data));
        setBitMapPH(pageToInsert.data, b);
        bitmap_deallocate(b);
    }
    //find free slot using pageHeader bitMap
    bitmap * b = getBitMapPH(pageToInsert.data);
    unsigned short nextFreeSlot = findFreeSlot(b);
    if(nextFreeSlot==getNumSlotsPerPage(pageFileHeader.data))
        return RC_RM_NO_FREE_PAGES;
    //update record->id.slot
    record->id.slot = nextFreeSlot;
    //read location of next free slot from current slot
    int recordSize = getRecordSize(rel->schema);
    //maybe put this into a macro
    char * slotPtr = pageToInsert.data;
    slotPtr+= 2*pageNumOffset + 2* sizeof(int);
    slotPtr+= bitmapOffset(getBitMapWordsPH(pageToInsert.data)); // add the bitMap size offset
    slotPtr+= (nextFreeSlot * recordSize );
    //write record->data to current slot
    //maybe we don't need to do recordSize +1 because we don't want the null char
    //at the end of the record data array
    memcpy(slotPtr, record->data,recordSize);
    //update the bitMap
    bitmap_set(b, nextFreeSlot);
    setBitMapArrayPH(pageToInsert.data, b);

    //check if the page doesn't have anymore slots
    //so the page will be removed from the linked list
    if(findFreeSlot(b)==getNumSlotsPerPage(pageFileHeader.data))
    {
        ASSERT_RC_OK(deleteFromFreeLinkedList(pageFileHeader.data,pageToInsert.data,bm));
    }
    //free bitmap
    bitmap_deallocate(b);
    //increment numTuples in the pageFile header
    setNumTuplesPF(pageFileHeader.data, getNumTuplesPF(pageToInsert.data)+1);
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
RC deleteRecord (RM_TableData *rel, RID id)
{
    RC returnCode = RC_INIT;
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
    char* pfhr = pageFileHeader.data;
    ASSERT_RC_OK(pinPage(bm,&pageToDelete,pageNum));
    //find free slot using pageHeader bitMap
    char * phr = pageToDelete.data;
    int recordSize =  getRecordSize(rel->schema);
    //delete the record at id.slot
    //(offset by slot*recordSize+sizeof(short))
    char * slotPtr = phr;
    slotPtr+= 2*pageNumOffset + 2* sizeof(int);
    slotPtr+= bitmapOffset(getBitMapWordsPH(phr)); // add the bitMap size offset
    slotPtr+= (slotNum * recordSize );

    //**if the page didn't previously have a free slot, check
    //to see if it is now the first page with a free slot
    //and update pageFile header appropriately
    bitmap * b = getBitMapPH(phr);
    if(findFreeSlot(b)==getNumSlotsPerPage(pageFileHeader.data))
    {
        //save the current pageNum into the prev ptr
        setPrevFreePagePH(phr,0);
        ASSERT_RC_OK(appendToFreeLinkedList(pfhr,phr,bm));
    }
    //update bitMap
    bitmap_clear(b, slotNum);
    setBitMapPH(phr, b);
    bitmap_deallocate(b);
    //Not sure if this is truly necessary
    //if we update the bitMap then we won't read from that slot anymore
    //this is just for safety and can be taken out
    memset(slotPtr, 0,recordSize);
    //decrement numTuples
    setNumTuplesPF(pfhr,getNumTuplesPF(pfhr)-1);
    //mark pages as dirty
    ASSERT_RC_OK(markDirty(bm, &pageFileHeader));
    ASSERT_RC_OK(markDirty(bm, &pageToDelete));
    //unpin the pageFile header and the page we inserted the record into
    ASSERT_RC_OK(unpinPage(bm,&pageFileHeader));
    ASSERT_RC_OK(unpinPage(bm,&pageToDelete));
    //we shouldn't need to worry about writing it back to disk
    //that will be handled by page replacement
    return RC_OK;
}

/*********************************************************************
updateRecord replaces the data in a slot with the data in *record
INPUT:
    *rel: initialized RM_TableData to update the record of
    *record: id contains page and slot, *data contains record to insert
*********************************************************************/
RC updateRecord (RM_TableData *rel, Record *record)
{
    RC returnCode = RC_INIT;
    //validate input
    if(!rel)
        return RC_RM_INIT_ERROR;
    if(!record)
        return RC_RM_INIT_ERROR;

    int pageNum = record->id.page;
    int slotNum = record->id.slot;
    //create two local BM_PageHandles
    BM_PageHandle pageToUpdate;
    BM_BufferPool* bm = rel->bufferPool;
    //pin the first page with a free slot
    ASSERT_RC_OK(pinPage(bm,&pageToUpdate,pageNum));
    char * phr = pageToUpdate.data;
    //read location of next free slot from current slot
    int recordSize = getRecordSize(rel->schema);
    //maybe put this into a macro
    char * slotPtr = phr;
    slotPtr+= 2*pageNumOffset + 2* sizeof(int);
    slotPtr+= bitmapOffset(getBitMapWordsPH(phr)); // add the bitMap size offset
    slotPtr+= (slotNum * recordSize );
    //write record->data to current slot
    //maybe we don't need to do recordSize +1 because we don't want the null char
    //at the end of the record data array
    memcpy(slotPtr,record->data,recordSize);
    //mark pages as dirty
    ASSERT_RC_OK(markDirty(bm, &pageToUpdate));
    //unpin the pageFile header and the page we inserted the record into
    ASSERT_RC_OK(unpinPage(bm,&pageToUpdate));
    //we shouldn't need to worry about writing it back to disk
    //that will be handled by page replacement
    return RC_OK;
}

/*********************************************************************
getRecord retrieves the data id.page and id.slot and initializes *record
INPUT:
    *rel: initialized RM_TableData to retrieve the record from
    id: contains the page and slot of the record of interest
    *record: allocated, but uninitiated Record to store data in
*********************************************************************/
RC getRecord (RM_TableData *rel, RID id, Record *record)
{
    RC returnCode = RC_INIT;
    //validate input
    if(!rel)
        return RC_RM_INIT_ERROR;
    if(!record)
        return RC_RM_INIT_ERROR;

    int pageNum = id.page;
    int slotNum = id.slot;
    //create local BM_PageHandles
    BM_PageHandle pageToGet;
    BM_BufferPool* bm = rel->bufferPool;
    //pin the page of interest
    ASSERT_RC_OK(pinPage(bm,&pageToGet,pageNum));
    //find free slot using pageHeader bitMap
    char * phr = pageToGet.data;
    //read location of next free slot from current slot
    int recordSize = getRecordSize(rel->schema);
    //maybe put this into a macro
    char * slotPtr = phr;
    slotPtr+= 2*pageNumOffset + 2* sizeof(int);
    slotPtr+= bitmapOffset(getBitMapWordsPH(phr)); // add the bitMap size offset
    slotPtr+= (slotNum * recordSize );
    //write record->data to current slot
    //maybe we don't need to do recordSize +1 because we don't want the null char
    //at the end of the record data array
    memcpy(record->data,slotPtr,recordSize);
    record->id.page = pageNum;
    record->id.slot = slotNum;
    //unpin the page we of the record we got
    ASSERT_RC_OK(unpinPage(bm,&pageToGet));
    //we shouldn't need to worry about writing it back to disk
    //that will be handled by page replacement
    return RC_OK;
}

/*********************************************************************
*
*                        SCAN FUNCTIONS
*
*********************************************************************/
/*********************************************************************
startScan: Initializes the RM_ScanHandle data structure
INPUT: initialized relation, instance of ScanHandle, and the condition
RETURNS: RC_OK
*********************************************************************/
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
    //Validation of inputs
    if(!rel || !scan || !cond)  //If input is invalid then return error code
        return RC_RM_INIT_ERROR;

    scan->mgmtData = cond;  //Store the condition into the mgmtData field
    scan->rel = rel;        //Store the relation into the rel field
    scan->slotNum = 0;
    scan->pageNum = 1;
    return RC_OK;
}

/*********************************************************************
next: Looks for the next tuple that fulfills the scan condition and
returns it.
INPUT: Instance of ScanHandle (if NULL is passed, then all tuples of
       the table should be returned), a Record
Return: RC_RM_NO_MORE_TUPLES once scan is completed
        RC_OK otherwise
*********************************************************************/
RC next (RM_ScanHandle *scan, Record *record)
{
    RC returnCode = RC_INIT;
    SM_FileHandle fHandle;
    BM_PageHandle pageFileHeader;
    BM_PageHandle curPage;//used to pin page to BufferPool
    BM_BufferPool* bm = scan->rel->bufferPool;
    //Validation of inputs
    if(!record)    //If input is invalid then return error code
        return RC_RM_INIT_ERROR;

    // open the page file
    ASSERT_RC_OK(openPageFile(scan->rel->name, &fHandle));
    //store number of pages in the fHandle into numPages
    int numPages = fHandle.totalNumPages;
    ASSERT_RC_OK(closePageFile(&fHandle));
    //pin the pageFile header to bufferpool
    //Only need to do once
    ASSERT_RC_OK(pinPage(bm,&pageFileHeader,0));
    char* pfhr = pageFileHeader.data;
    Value *result;
    //Iterate through the pages on disk and pin to bufferpool and search over bitmap of that page
    for(; scan->pageNum<numPages; scan->pageNum++)
    {
        ASSERT_RC_OK(pinPage(bm,&curPage,scan->pageNum));
        char * phr = curPage.data;//used to find used slot
        //point to bitmap in the current page
        //TODO: have address of bitmap be pointed by a variable (lets call it bitmap just for use in while loop)
        bitmap * b = getBitMapPH(phr);
        //while we did not reach the end of the slot
        for(; scan->slotNum < getNumSlotsPerPage(pfhr); ++scan->slotNum)
        {
            //utilize bitmap_read(bitmap, int n) to retrieve bit
            //if bit = 1, then store the data (record) at that position in the page into record->id.slot
            if(bitmap_read(b,scan->slotNum)==1)
            {
                RID rid;
                rid.page = scan->pageNum;
                rid.slot = scan->slotNum;
                ASSERT_RC_OK(getRecord(scan->rel,rid,record));
                ASSERT_RC_OK(evalExpr(record, scan->rel->schema, scan->mgmtData, &result));
                if(result->v.boolV || scan->mgmtData == NULL)
                {
                    //return record
                    record->id.page = scan->pageNum;
                    record->id.slot = scan->slotNum;
                    freeVal(result);
                    bitmap_deallocate(b);
                    ++scan->slotNum;
                    return RC_OK;
                }
            }
        }
        bitmap_deallocate(b);
    }
    freeVal(result);
    return RC_RM_NO_MORE_TUPLES;
}

/*********************************************************************
closeScan: Finishes the scan
INPUT: Instance of ScanHandle
RETURNS: RC_OK
*********************************************************************/
RC closeScan (RM_ScanHandle *scan)
{
    /*free(scan->mgmtData);
    scan->mgmtData = NULL;*/
    return RC_OK;
}

/*********************************************************************
*
*                        SCHEMA FUNCTIONS
*
*********************************************************************/
/*********************************************************************
getRecordSize sums the typeLengths of the attributes to determine
recordSize
INPUT: initialized Schema
RETURNS: the size of a single record for the provided schema
*********************************************************************/
int getRecordSize (Schema *schema)
{
    int recordSize = 0;
    for(int i = 0; i < schema->numAttr; i++)
    {
        recordSize += schema->typeLength[i];
    }
    return recordSize;
}
/*********************************************************************
createSchema allocates memory for a Schema struct initializes all
variables to the parameters passed to the function
INPUT: Initialized values for all members of the Schema struct
    **attrNames: an array of null terminated strings
    *typeLength: size in bytes of string type attributes, not
        including the null terminator. Zero otherwise
*********************************************************************/
Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes,
                      int *typeLength, int keySize, int *keys)
{
    //Allocate memory for all arrays in schema struct
    VALID_CALLOC(Schema, schema, 1, sizeof(Schema));

    //typeLength only includes the length of string dataTypes
    //update typeLength with length of other dataTypes
    for(int i = 0; i < numAttr; i++){
        switch (dataTypes[i]){
        case DT_INT:
            typeLength[i] = sizeof(int);
            break;
        case DT_FLOAT:
            typeLength[i] = sizeof(float);
            break;
        case DT_BOOL:
            typeLength[i] = sizeof(bool);
            break;
        }
    }
    //Store non-array inputs
    schema->numAttr = numAttr;
    schema->keySize = keySize;
    //Store array inputs in schema struct
    schema->attrNames = attrNames;
    schema->dataTypes = dataTypes;
    schema->typeLength = typeLength;
    schema->keyAttrs = keys;

    return schema;
}
/*********************************************************************
freeSchema frees all memory associated with a Schema struct, including
the memory for the struct itself
INPUT: initialized Schema
*********************************************************************/
RC freeSchema (Schema *schema)
{
    for(int i = 0; i < schema->numAttr; i++)
    {
        free(schema->attrNames[i]);
        schema->attrNames[i] = NULL;
    }
    free(schema->dataTypes);
    schema->dataTypes = NULL;
    free(schema->typeLength);
    schema->typeLength = NULL;
    free(schema->keyAttrs);
    schema->keyAttrs = NULL;
    free(schema);
    schema = NULL;
    return RC_OK;
}

/*********************************************************************
*
*                        ATTRIBUTE FUNCTIONS
*
*********************************************************************/
/*********************************************************************
Allocates memory for a Record struct along with the the memory for
a table record which is equivalent to the sum of the size of all the
attributes specified in the schema.
If address of record or schema does not exist, return INIT error
INPUT:
	**record: pointer to a pointer of the newly allocated memory
			  for Record
	*schema: pointer to pre initialized schema
*********************************************************************/
RC createRecord (Record **record, Schema *schema)
{
    //Validate passed input
    if(!record || !schema)
        return RC_RM_INIT_ERROR;
    //Allocate memory for the new record and its data
    //And initialize to all 0's
    VALID_CALLOC(Record, rec, 1, sizeof(Record));
    rec->data = (char*) calloc(1, getRecordSize(schema));
    *record = rec;

    return RC_OK;
}

/*********************************************************************
Frees all memory associated with Record struct along with the
the memory for the struct
INPUT:
	*record: pointer of the populated record struct
*********************************************************************/
RC freeRecord (Record *record)
{
    free(record->data);
    record->data = NULL;
    free(record);
    record = NULL;

    return RC_OK;
}

/*********************************************************************
Gets the value of an attribute(specified by 'attrNum') from the
record and stores in *value
INPUT:
	*record: initialized record to retrieve the attribute from from
	*schema: initialized schema - used for locating the attribute in record
	attrNum: The index of the attribute in the record
	**value: the pointer where the retrieved attribute value is stored
*********************************************************************/
RC getAttr (Record *record, Schema *schema, int attrNum, Value **value)
{
    char *attr_offset = record->data + getAttrOffset(schema, attrNum);

    //Value *attr_val = (Value *) malloc(sizeof(Value));
    VALID_CALLOC(Value, attr_val, 1, sizeof(Value));

    attr_val->dt = schema->dataTypes[attrNum];

    // use memcpy
    switch(schema->dataTypes[attrNum])
    {
    case DT_INT:
        memcpy(&(attr_val->v.intV), attr_offset, schema->typeLength[attrNum]);
        break;
    case DT_STRING:
        attr_val->v.stringV =(char *) calloc(schema->typeLength[attrNum]+1, sizeof(char));
        memcpy(attr_val->v.stringV, attr_offset, schema->typeLength[attrNum]);
        break;
    case DT_FLOAT:
        memcpy(&(attr_val->v.floatV), attr_offset, schema->typeLength[attrNum]);
        break;
    case DT_BOOL:
        memcpy(&(attr_val->v.boolV), attr_offset, schema->typeLength[attrNum]);
        break;
    }

    *value = attr_val;
    return RC_OK;
}

/*********************************************************************
Sets the value of an attribute(specified by 'attrNum') from the
*value to the *record
INPUT:
	*record: initialized record where value is to be updated
	*schema: initialized schema - used for locating the attribute in record
	attrNum: The index of the attribute in the record to be updated
	*value: pointer to the new value the record is to be updated with
*********************************************************************/
RC setAttr (Record *record, Schema *schema, int attrNum, Value *value)
{
    char *attr_offset = record->data + getAttrOffset(schema, attrNum);

    switch(schema->dataTypes[attrNum])
    {
    case DT_INT:
    {
        memcpy(attr_offset, &(value->v.intV), schema->typeLength[attrNum]);
        break;
    }
    case DT_STRING:
    {
        memcpy(attr_offset, value->v.stringV, schema->typeLength[attrNum]);
        break;
    }
    case DT_FLOAT:
    {
        memcpy(attr_offset, &(value->v.floatV), schema->typeLength[attrNum]);
        break;
    }
    case DT_BOOL:
    {
        memcpy(attr_offset, &(value->v.boolV), schema->typeLength[attrNum]);
        break;
    }
    }

    return RC_OK;
}

/*********************************************************************
*
*                        HELPER FUNCTIONS
*
*********************************************************************/

/*********************************************************************
checks if the bitMap has a free slot if not it return bitMap size as
the next free slot index
*********************************************************************/
int static findFreeSlot(bitmap * bitMap)
{
    int mapSize = bitMap->bits;
    for(int i=0; i<mapSize; i++)
    {
        if(bitmap_read(bitMap,i)==0)
        {
            return i;
        }
    }
    return mapSize;
}
/********************************************************************
find the page number for the next page to be allocated upon creation
********************************************************************/
static RC findNewPageNum(RM_TableData * rel, unsigned int * nextFreePage)
{
    RC returnCode = RC_INIT;
    SM_FileHandle fHandle;
    ASSERT_RC_OK(openPageFile(rel->name,&fHandle));
    *nextFreePage = fHandle.totalNumPages;
    ASSERT_RC_OK(closePageFile(&fHandle));
    return RC_OK;
}
/*********************************************************************
preparePFHdr populates a PageHandle with the data for the PageFile Hdr
Assumes initial generation of pageFile, so no tuples have been added
INPUT:
    *schema: fully initialized Schema struct
    *pHandle: uninitialized PageHandle with PAGE_SIZE memory alloc'd
FORMAT:
DataType and TypeLength pairs are repeated numAttr times
keyAttr is repeated keySize times
strlen is the length of the subsequent attribute name
strlen and attrName pairs are repeated numAttr times

---------------------------------------------------------------------------
ushort recordSize | uint numTuples | uint nextFreePage |
---------------------------------------------------------------------------
ushort numSlotsPerPage | ushort schemaSize | ushort numAttr |
---------------------------------------------------------------------------
ushort DataType | ushort TypeLength | ushort DataType | ushort TypeLength |
---------------------------------------------------------------------------
... | ushort keySize | ushort keyAttr | ushort keyAttr | ... |
---------------------------------------------------------------------------
ushort strlen | char* attrName | ushort strlen | char* attrName | ... |
---------------------------------------------------------------------------
OFFSETS:
    recordSize: 0
    numTuples: sizeof(ushort)
    nextFreePage: sizeof(ushort) + sizeof(uint)
    numSlotsPerPage: sizeof(ushort) + 2*sizeof(uint)
    schemaSize: 2*( sizeof(ushort) + sizeof(uint) )
    schema: 3*sizeof(ushort) + 2*sizeof(uint)

SCHEMA OFFSETS FROM START OF SCHEMA
    numAttr: 0
    DataType of attribute i: 2*i*sizeof(ushort) + sizeof(ushort)
    TypeLength of attribute i: 2*(i+1)*sizeof(ushort)
    keySize: (2*numAttr + 1) * sizeof(ushort)
    keyAttr i: (2*numAttr + 2 + i) * sizeof(ushort)
    names: (2*numAttr + 2 + keySize * i) * sizeof(ushort)
    Offset to a specific attribute's name will need to be calculated
        using the strlen's
*********************************************************************/
static RC preparePFHdr(Schema *schema, char *pHandle)
{
    //validate input
    if(!schema || !pHandle)
        return RC_RM_INIT_ERROR;
    //Generate Info for Header
    unsigned short recordSize = (unsigned short) getRecordSize(schema);
    unsigned int numTuples = 0;
    unsigned int nextFreePage = 0;
    //numSlotsPerPage accounts for the bitmap and next and prev pointers
    unsigned short numSlotsPerPage = calcNumSlotsPerPage(recordSize);
    //Retrieve existing data from schema
    unsigned short numAttr = (unsigned short) schema->numAttr;
    unsigned short keySize = (unsigned short) schema->keySize;
    //Allocate memory for the schema arrays
    VALID_CALLOC(unsigned short, typeAndLength, 2*numAttr, sizeof(unsigned short));
    VALID_CALLOC(unsigned short, keyAttrs, keySize, sizeof(unsigned short));
    //Initialize variables to use for retrieving string data from schema
    unsigned short sizeOfAttrNames = 0;
    VALID_CALLOC(unsigned short, strLen, numAttr, sizeof(unsigned short));
    //Retrieve data from arrays and format how we want
    for(int i = 0; i < numAttr; i++)
    {
        typeAndLength[2*i] = (unsigned short) schema->dataTypes[i];
        typeAndLength[(2*i)+1] = (unsigned short) schema->typeLength[i];
        strLen[i] = (unsigned short) strlen(schema->attrNames[i]);
        sizeOfAttrNames += strLen[i];
    }
    //Calculate the schema size from its components
    unsigned short schemaSize = (2 + 3*numAttr + keySize) * sizeof(unsigned short) + sizeOfAttrNames;
    //Initialize an offset pointer for the write location
    char* curOffset = (char*) pHandle;
    //Populating the pageHandle with the data
    memcpy(curOffset, &recordSize, sizeof(recordSize));
    curOffset += sizeof(recordSize);
    memcpy(curOffset, &numTuples, sizeof(numTuples));
    curOffset += sizeof(numTuples);
    memcpy(curOffset, &nextFreePage, sizeof(nextFreePage));
    curOffset += sizeof(nextFreePage);
    memcpy(curOffset, &numSlotsPerPage, sizeof(numSlotsPerPage));
    curOffset += sizeof(numSlotsPerPage);
    memcpy(curOffset, &schemaSize, sizeof(schemaSize));
    curOffset += sizeof(schemaSize);
    memcpy(curOffset, &numAttr, sizeof(numAttr));
    curOffset += sizeof(numAttr);
    //Populating the pageHandle with array data
    for(int i = 0; i < 2*numAttr; i++)
    {
        memcpy(curOffset, &typeAndLength[i], sizeof(typeAndLength[i]));
        curOffset += sizeof(typeAndLength[i]);
    }
    //Populating the pageHandle with data
    memcpy(curOffset, &keySize, sizeof(keySize));
    curOffset += sizeof(keySize);
    //Populating the pageHandle with array data
    unsigned short keyAttr;
    for(int i = 0; i < keySize; i++)
    {
        keyAttr = (unsigned short)schema->keyAttrs[i];
        memcpy(curOffset, &keyAttr, sizeof(keyAttr));
        curOffset += sizeof(keyAttr);
    }
    for(int i = 0; i < numAttr; i++)
    {
        memcpy(curOffset, &strLen[i], sizeof(strLen[i]));
        curOffset += sizeof(strLen[i]);
        memcpy(curOffset, &schema->attrNames[i], strLen[i]);
        curOffset += strLen[i];
    }
    //Freeing memory
    free(typeAndLength);
    free(keyAttrs);
    free(strLen);
    return RC_OK;
}


static RC appendToFreeLinkedList(char * pfhr,
                                 char * phr,
                                 BM_BufferPool * bm)
{
    RC returnCode = RC_INIT;
    //update nextPage, prevPage
    BM_PageHandle nextPageHandle;
    //this page is the header
    //update the next of page header to previous pageFileheader next
    setNextFreePagePH(phr,getNextFreePage(pfhr));
    //update pageFileHeader
    setNextFreePage(pfhr, getPrevFreePagePH(phr));//current page Number
    //if the list isn't empty
    //update ref of the old head
    if(getNextFreePage(pfhr) != 0)
    {
        //update next page Header
        ASSERT_RC_OK(pinPage(bm, &nextPageHandle, getNextFreePagePH(phr)));
        //update the head
        setPrevFreePagePH(nextPageHandle.data,getPrevFreePagePH(phr));
        ASSERT_RC_OK(markDirty(bm,&nextPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&nextPageHandle));
    }
    //update the prev of head to pageFileHeader
    setPrevFreePagePH(phr, 0);
    return RC_OK;
}

static RC deleteFromFreeLinkedList(char* pfhr,
                                   char *phr,
                                   BM_BufferPool*bm)
{
    RC returnCode = RC_INIT;

    //update nextPage, prevPage
    BM_PageHandle nextPageHandle;
    BM_PageHandle prevPageHandle;
    //this page is the header
    if(getPrevFreePagePH(phr) == 0)
    {
        //update pageFileHeader
        setNextFreePage(pfhr, getNextFreePagePH(phr) );
    }
    else
    {
        //update next page Header
        ASSERT_RC_OK(pinPage(bm, &prevPageHandle, getPrevFreePagePH(phr)));
        //update the head
        setNextFreePagePH(prevPageHandle.data, getNextFreePagePH(phr));
        ASSERT_RC_OK(markDirty(bm,&prevPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&prevPageHandle));
    }
    //if the page is not the tail
    if(getNextFreePagePH(phr) != 0)
    {
        //update next page Header
        ASSERT_RC_OK(pinPage(bm, &nextPageHandle, getNextFreePagePH(phr)));
        //update the head
        setPrevFreePagePH(nextPageHandle.data,getPrevFreePagePH(phr));
        ASSERT_RC_OK(markDirty(bm,&nextPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&nextPageHandle));
    }
    //update the current page next and prev
    setNextFreePagePH(phr,0);
    setPrevFreePagePH(phr,0);
    return RC_OK;
}

/*********************************************************************
Gets the byte offset value of an attribute in the record for setting
new value to the attribute or for retrieving the value from the record
INPUT:
	*schema: initialized schema - used for locating the attribute in record
	attrNum: The index of the attribute in the record
*********************************************************************/
static int getAttrOffset(Schema *schema, int attrNum)
{
    int offset = 0;
    for(int i = 0; i < attrNum; i++)
    {
        offset += schema->typeLength[i];
    }
    return offset;
}
/*********************************************************************
calcNumSlotsPerPage solves the following equation iteratively

PAGE_SIZE >= 4*i + floor((n+31)/32)/4 + n*r
where i is sizeof(unsigned int) to account for ints in header
where r is the size of a record for a given schema
where n is the number of slots per page

floor((n+31)/32) is used since the bitmap is stored in word-sized chunks
floor((n+31)/32) is divided by 4 to convert from words to bytes
Calculation assumes 32 bit words

INPUT: recordSize: size of a record for a given schema
*********************************************************************/
static unsigned short calcNumSlotsPerPage(unsigned short recordSize)
{
    //Calculates numSlotsPerPage assuming the number of bits in the
    //bitmap exactly equals the numSlotsPerPage.
    //Solves: PAGE_SIZE >= 4*i + n + n*r
    unsigned short numSlotsPerPage = (PAGE_SIZE - 4*sizeof(unsigned int)) / (recordSize + 0.125);
    //Rounds the number of bytes used by the bitmap up to the next word
    unsigned short numBytesForBitmap = ((numSlotsPerPage+31)/32)*4;
    //Recalculates numSlotsPerPage with the larger header
    numSlotsPerPage = (PAGE_SIZE - 4*sizeof(unsigned int) - numBytesForBitmap) / recordSize;
    return numSlotsPerPage;
}
/*********************************************************************
*
*               PAGE HEADER GETTERS AND SETTERS
*
*********************************************************************/
static unsigned int getPrevFreePagePH(char *phrFrame)
{
    unsigned int prevFreePage;
    memcpy(&prevFreePage, phrFrame + 0, pageNumOffset);
    return prevFreePage;
}

static unsigned int getNextFreePagePH(char *phrFrame)
{
    unsigned int nextFreePage;
    memcpy(&nextFreePage, phrFrame + pageNumOffset, pageNumOffset);
    return nextFreePage;
}

static void setPrevFreePagePH(char * phrFrame, unsigned int pageNum)
{
    memcpy(phrFrame + 0, &pageNum,pageNumOffset);
}

static void setNextFreePagePH(char * phrFrame, unsigned int pageNum)
{
    memcpy(phrFrame + pageNumOffset, &pageNum,pageNumOffset);
}

static bitmap* getBitMapPH(char * phrFrame)
{
    VALID_CALLOC(bitmap, b,1,sizeof(bitmap));
    char * curOff = phrFrame + 2 * pageNumOffset;
    //set number of bits
    memcpy(&b->bits,curOff, sizeof(int));
    curOff+=sizeof(int);
    //set number of words
    memcpy(&b->words,curOff, sizeof(int));
    curOff +=sizeof(int);
    //set bit Array
    VALID_CALLOC(bitmap_type *, bitArray, b->words,sizeof(bitmap_type));
    memcpy(bitArray,curOff, sizeof(bitmap_type)* b->words);
    b->array =(bitmap_type*) bitArray;
    return b;
}

static int getBitMapWordsPH(char* phrFrame)
{
    int words;
    memcpy(&words, phrFrame + sizeof(int)+ 2 * pageNumOffset, sizeof(int));
    return words;
}

static int getBitMapBitsPH(char* phrFrame)
{
    int bits;
    memcpy(&bits, phrFrame+ 2 * pageNumOffset, sizeof(int));
    return bits;
}

static void setBitMapPH(char * phrFrame, bitmap* b)
{
    char * curOff = phrFrame + 2 * pageNumOffset;

    //set number of bits
    memcpy(curOff,&b->bits, sizeof(int));
    curOff+=sizeof(int);
    //set number of words
    memcpy(curOff,&b->words, sizeof(int));
    curOff +=sizeof(int);
    //set bit Array
    memcpy(curOff, b->array, sizeof(bitmap_type)* b->words);
}

static void setBitMapArrayPH(char* phrFrame, bitmap * b)
{
    char * curOff = phrFrame + 2 * pageNumOffset + 2 * sizeof(int);
    memcpy(curOff, b->array, sizeof(bitmap_type)* b->words);
}

/*********************************************************************
*
*               PAGEFILE HEADER GETTERS AND SETTERS
*
*********************************************************************/
static unsigned short getRecordSizePF(char *pfHdrFrame)
{
    unsigned short recordSize;
    memcpy(&recordSize, pfHdrFrame + recordSizeOffset, sizeof(unsigned short));
    return recordSize;
}
static unsigned int getNumTuplesPF(char *pfHdrFrame)
{
    unsigned int numTuples;
    memcpy(&numTuples, pfHdrFrame + numTuplesOffset, sizeof(unsigned int));
    return numTuples;
}
static void setNumTuplesPF(char *pfHdrFrame, unsigned int numTuples)
{
    memcpy(pfHdrFrame + numTuplesOffset, &numTuples, sizeof(unsigned int));
}
static unsigned int getNextFreePage(char *pfHdrFrame)
{
    unsigned int nextFreePage;
    memcpy(&nextFreePage, pfHdrFrame + nextFreePageOffset, sizeof(unsigned int));
    return nextFreePage;
}
static void setNextFreePage(char *pfHdrFrame, unsigned int nextFreePage)
{
    memcpy(pfHdrFrame + nextFreePageOffset, &nextFreePage, sizeof(unsigned int));
}
static unsigned short getNumSlotsPerPage(char *pfHdrFrame)
{
    unsigned short numSlotsPerPage;
    memcpy(&numSlotsPerPage, pfHdrFrame + numSlotsPerPageOffset, sizeof(unsigned short));
    return numSlotsPerPage;
}
static unsigned short getSchemaSize(char *pfHdrFrame)
{
    unsigned short schemaSize;
    memcpy(&schemaSize, pfHdrFrame + schemaSizeOffset, sizeof(unsigned short));
    return schemaSize;
}
static void getSchema(char *pfHdrFrame, Schema *schema)
{
    //store values of non-array variables
    schema->numAttr = getNumAttr(pfHdrFrame);
    schema->keySize = getKeySize(pfHdrFrame);
    // allocate memory for variable length arrays in schema struct
    VALID_CALLOC(char*, attrNames, schema->numAttr, sizeof(char*));
    VALID_CALLOC(DataType, dataTypes, schema->numAttr, sizeof(DataType));
    VALID_CALLOC(int, typeLength, schema->numAttr, sizeof(int));
    VALID_CALLOC(int, keyAttrs, schema->keySize, sizeof(int));
    //store values and pointers in schema struct
    schema->attrNames = attrNames;
    schema->dataTypes = dataTypes;
    schema->typeLength = typeLength;
    schema->keyAttrs = keyAttrs;
    //Populate arrays in the schema
    for(int i = 0; i < schema->numAttr; i++)
    {
        schema->dataTypes[i] = getIthDataType(pfHdrFrame, i);
        schema->typeLength[i] = getIthTypeLength(pfHdrFrame, i);
        schema->attrNames[i] = getIthAttrName(pfHdrFrame, i);
    }
    for(int i = 0; i < schema->keySize; i++)
    {
        keyAttrs[i] = getIthKeyAttr(pfHdrFrame, i);
    }
}
static unsigned short getNumAttr(char *pfHdrFrame)
{
    unsigned short numAttr;
    memcpy(&numAttr, pfHdrFrame + numAttrOffset, sizeof(unsigned short));
    return numAttr;
}
static DataType getIthDataType(char *pfHdrFrame, int ithSlot)
{
    unsigned short dataType;
    memcpy(&dataType, pfHdrFrame + dataTypeOffset(ithSlot), sizeof(unsigned short));
    switch(dataType)
    {
    case 0:
        return DT_INT;
    case 1:
        return DT_STRING;
    case 2:
        return DT_FLOAT;
    case 3:
        return DT_BOOL;
    default:
        return DT_INT;
    }
}
static unsigned short getIthTypeLength(char *pfHdrFrame, int ithSlot)
{
    unsigned short typeLength;
    memcpy(&typeLength, pfHdrFrame + typeLengthOffset(ithSlot), sizeof(unsigned short));
    return typeLength;
}
static unsigned short getKeySize(char *pfHdrFrame)
{
    unsigned short keySize;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    memcpy(&keySize, pfHdrFrame + keySizeOffset, sizeof(unsigned short));
    return keySize;
}
static unsigned short getIthKeyAttr(char *pfHdrFrame, int ithSlot)
{
    unsigned short keyAttr;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    memcpy(&keyAttr, pfHdrFrame + keyAttrOffset(ithSlot), sizeof(unsigned short));
    return keyAttr;
}
static unsigned short getIthNameLength(char *pfHdrFrame, int ithSlot)
{
    unsigned short nameLength;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    unsigned short keySize = getKeySize(pfHdrFrame);
    char *ptr = pfHdrFrame + attrNamesOffset;
    for(int i = 0; i <= ithSlot; i++)
    {
        memcpy(&nameLength, ptr, sizeof(unsigned short));
        ptr = ptr + nameLength + sizeof(unsigned short);
    }
    return nameLength;
}
static char* getIthAttrName(char *pfHdrFrame, int ithSlot)
{
    unsigned short nameLength;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    unsigned short keySize = getKeySize(pfHdrFrame);
    char *ptr = pfHdrFrame + attrNamesOffset + sizeof(unsigned short);
    for(int i = 0; i < ithSlot; i++)
    {
        nameLength = getIthNameLength(pfHdrFrame, i);
        ptr = ptr + nameLength + sizeof(unsigned short);
    }
    nameLength = getIthNameLength(pfHdrFrame, ithSlot);
    VALID_CALLOC(char, attrName, 1, nameLength+1); //room for null char
    memcpy(attrName, ptr, nameLength);
    return attrName;
}
