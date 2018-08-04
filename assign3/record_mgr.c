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
#define nextFreePageOffset numTuplesOffset + sizeof(unsigned int)
#define numSlotsPerPageOffset nextFreePageOffset + sizeof(unsigned int)
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
static RC deleteFromFreeLinkedList(RM_PageFileHeader* pfhr,RM_PageHeader *phr, BM_BufferPool*bm);
static RC appendToFreeLinkedList(RM_PageFileHeader * pfhr, RM_PageHeader * phr,BM_BufferPool * bm);
static int getAttrOffset(Schema *schema, int attrNum);

// Prototypes for getters and setters for pagefile header data
static unsigned short getRecordSizePF(char *pfHdrFrame);
static void setRecordSizePF(char *pfHdrFrame, unsigned short recordSize);
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
RC openTable (RM_TableData *rel, char *name){
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
RC closeTable (RM_TableData *rel){
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
RC deleteTable (char *name){
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
int getNumTuples (RM_TableData *rel){
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
RC insertRecord (RM_TableData *rel, Record *record){
    RC returnCode = RC_INIT;
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
    RM_PageFileHeader* pfhr = (RM_PageFileHeader *) &pageFileHeader.data;
    //find the first page with free slot from pageFile header
    int freePageNum = pfhr->nextFreePage;
    if(freePageNum==NO_PAGE)
        return RC_RM_NO_FREE_PAGES;
    //update record->id.page
    record->id.page = freePageNum;
    //pin the first page with a free slot
    ASSERT_RC_OK(pinPage(bm,&pageToInsert,freePageNum));
    //find free slot using pageHeader bitMap
    RM_PageHeader * phr = (RM_PageHeader *) &pageToInsert.data;
    int nextFreeSlot = findFreeSlot(phr->freeBitMap);
    if(nextFreeSlot==phr->freeBitMap->bits)
        return RC_RM_NO_FREE_PAGES;
    //update record->id.slot
    record->id.slot = nextFreeSlot;
    //read location of next free slot from current slot
    int recordSize = getRecordSizePF(pageFileHeader.data);
    //maybe put this into a macro
    char * slotPtr = (char*) &pageToInsert+sizeof(RM_PageHeader)
    + bitmapOffset(phr->freeBitMap->words) // add the bitMap size offset
    + (nextFreeSlot * recordSize );
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
        ASSERT_RC_OK(deleteFromFreeLinkedList(pfhr,phr,bm));
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
    RM_PageFileHeader* pfhr = (RM_PageFileHeader *) &pageFileHeader.data;
    ASSERT_RC_OK(pinPage(bm,&pageToDelete,pageNum));
    //find free slot using pageHeader bitMap
    RM_PageHeader * phr = (RM_PageHeader *) &pageToDelete.data;
    int recordSize = getRecordSizePF(pageFileHeader.data);
    //delete the record at id.slot
        //(offset by slot*recordSize+sizeof(short))
    char * slotPtr = (char*) &pageToDelete+sizeof(RM_PageHeader)
    + bitmapOffset(phr->freeBitMap->words) // add the bitMap size offset
    + (slotNum * recordSize );

    //**if the page didn't previously have a free slot, check
    //to see if it is now the first page with a free slot
    //and update pageFile header appropriately
    if(findFreeSlot(phr->freeBitMap)==phr->freeBitMap->bits)
    {
        //save the current pageNum into the prev ptr
        phr->prevFreePage = pageNum;
        ASSERT_RC_OK(appendToFreeLinkedList(pfhr,phr,bm));
    }
    //update bitMap
    bitmap_clear(phr->freeBitMap, slotNum);
    //Not sure if this is truly necessary
    //if we update the bitMap then we won't read from that slot anymore
    //this is just for safety and can be taken out
    memset(slotPtr, 0,recordSize);
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
    return RC_OK;
}

/*********************************************************************
updateRecord replaces the data in a slot with the data in *record
INPUT:
    *rel: initialized RM_TableData to update the record of
    *record: id contains page and slot, *data contains record to insert
*********************************************************************/
RC updateRecord (RM_TableData *rel, Record *record){
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
    RM_PageHeader * phr = (RM_PageHeader *) &pageToUpdate.data;
    //read location of next free slot from current slot
    int recordSize = getRecordSize(rel->schema);
    //maybe put this into a macro
    char * slotPtr = (char*) &pageToUpdate+sizeof(RM_PageHeader)
    + bitmapOffset(phr->freeBitMap->words) // add the bitMap size offset
    + (slotNum * recordSize );
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
RC getRecord (RM_TableData *rel, RID id, Record *record){
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
    RM_PageHeader * phr = (RM_PageHeader *) &pageToGet.data;
    //read location of next free slot from current slot
    int recordSize = getRecordSize(rel->schema);
    //maybe put this into a macro
    char * slotPtr = (char*) &pageToGet+sizeof(RM_PageHeader)
    + bitmapOffset(phr->freeBitMap->words) // add the bitMap size offset
    + (slotNum * recordSize );
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
//initializes the RM_ScanHandle data structure passed as an argument to startScan.
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond){
    //Validation of inputs
    if(!rel || !scan || !cond)  //If input is invalid then return error code
        return RC_RM_INIT_ERROR;

    scan->mgmtData = cond;  //Store the condition into the mgmtData field
    scan->rel = rel;        //Store the relation into the rel field
    return RC_OK;
}
//next method should return the next tuple that fulfills the scan condition
//If NULL is passed as a scan condition, then all tuples of the table should be returned
RC next (RM_ScanHandle *scan, Record *record){
    //Validation of inputs
    if(!scan || !record)    //If input is invalid then return error code
      return RC_RM_INIT_ERROR;

//    if()

    return RC_OK;
    //next should return RC_RM_NO_MORE_TUPLES once the scan is completed and RC_OK otherwise
}
//Return RC_OK
RC closeScan (RM_ScanHandle *scan){
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
int getRecordSize (Schema *schema){
    int recordSize = 0;
    for(int i = 0; i < schema->numAttr; i++){
        recordSize += schema->typeLength[i];
    }
    return recordSize;
}
/*********************************************************************
createSchema allocates memory for a Schema struct and points each
member of the struct to the parameters passed to the function
INPUT: Initialized values for all members of the Schema struct
*********************************************************************/
Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys){
    VALID_CALLOC(Schema, schema, 1, sizeof(Schema));

    schema->numAttr = numAttr;
    schema->attrNames = attrNames;
    schema->dataTypes = dataTypes;
    schema->typeLength = typeLength;
    schema->keyAttrs = keys;
    schema->keySize = keySize;
    return schema;
}
/*********************************************************************
freeSchema frees all memory associated with a Schema struct, including
the memory for the struct itself
INPUT: initialized Schema
*********************************************************************/
RC freeSchema (Schema *schema){
    for(int i = 0; i < schema->numAttr; i++){
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
RC createRecord (Record **record, Schema *schema){
    //Validate passed input
    if(!record || !schema)
      return RC_RM_INIT_ERROR;
    //Allocate memory for the new record and its data
    //And initialize to all 0's
    VALID_CALLOC(Record, rec, 0, sizeof(Record));
    rec->data = (char*) malloc(getRecordSize(schema));
    *record = rec;

    return RC_OK;
}

/*********************************************************************
Frees all memory associated with Record struct along with the
the memory for the struct
INPUT:
	*record: pointer of the populated record struct
*********************************************************************/
RC freeRecord (Record *record){
	free(record->data);
	//record->id = NULL;
	free(record);

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
RC getAttr (Record *record, Schema *schema, int attrNum, Value **value){
	char *attr_offset = record->data + getAttrOffset(schema, attrNum);

	//Value *attr_val = (Value *) malloc(sizeof(Value));
	VALID_CALLOC(Value, attr_val, 1, sizeof(Value));

	attr_val->dt = schema->dataTypes[attrNum];

	// use memcpy
	switch(schema->dataTypes[attrNum]) {
		case DT_INT:{
			memcpy(&(attr_val->v.intV), attr_offset, schema->typeLength[attrNum]);
			//attr_val->v.intV = *(int *)attr_offset;
			break;
		}
		case DT_STRING: {
			memcpy(attr_val->v.stringV, attr_offset, schema->typeLength[attrNum]);
			break;
		}
		case DT_FLOAT: {
			memcpy(&(attr_val->v.floatV), attr_offset, schema->typeLength[attrNum]);
			//attr_val->v.floatV = *(float *)attr_offset;
			break;
		}
		case DT_BOOL: {
			memcpy(&(attr_val->v.boolV), attr_offset, schema->typeLength[attrNum]);
			//attr_val->v.boolV = *(bool *)attr_offset;
			break;
		}
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
RC setAttr (Record *record, Schema *schema, int attrNum, Value *value){
	char *attr_offset = record->data + getAttrOffset(schema, attrNum);

	switch(schema->dataTypes[attrNum]) {
		case DT_INT:{
			memcpy(attr_offset, &(value->v.intV), schema->typeLength[attrNum]);
			//*(int *)attr_offset = attr_val->v.intV;
			break;
		}
		case DT_STRING: {
			memcpy(attr_offset, value->v.stringV, schema->typeLength[attrNum]);
			break;
		}
		case DT_FLOAT: {
			memcpy(attr_offset, &(value->v.floatV), schema->typeLength[attrNum]);
			//*(float *)attr_offset = attr_val->v.floatV;
			break;
		}
		case DT_BOOL: {
			memcpy(attr_offset, &(value->v.boolV), schema->typeLength[attrNum]);
			//*(bool *)attr_offset = attr_val->v.boolV;
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
static RC preparePFHdr(Schema *schema, char *pHandle){
    //validate input
    if(!schema || !pHandle)
        return RC_RM_INIT_ERROR;

    //Generate Info for Header
    unsigned short recordSize = (unsigned short) getRecordSize(schema);
    unsigned int numTuples = 0;
    unsigned int nextFreePage = 0;
    //numSlotsPerPage accounts for the bitmap and next and prev pointers
    unsigned short numSlotsPerPage = (unsigned short) ((PAGE_SIZE - 2*sizeof(int)) / (recordSize + 0.125));

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
    for(int i = 0; i < numAttr; i++){
        typeAndLength[2*i] = (unsigned short) schema->dataTypes[i];
        typeAndLength[(2*i)+1] = (unsigned short) schema->typeLength[i];
        strLen[i] = (unsigned short) strlen(schema->attrNames[i]);
        sizeOfAttrNames += strLen[i];
    }

    //Calculate the schema size from its components
    unsigned short schemaSize = (2 + 3*numAttr + keySize) * sizeof(unsigned short) + sizeOfAttrNames;

    //Populating the pageHandle with the data
    char* curOffset = (char*) pHandle;

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

    for(int i = 0; i < 2*numAttr; i++){
        memcpy(curOffset, &typeAndLength[i], sizeof(typeAndLength[i]));
        curOffset += sizeof(typeAndLength[i]);
    }

    memcpy(curOffset, &keySize, sizeof(keySize));
    curOffset += sizeof(keySize);
    unsigned short keyAttr;

    for(int i = 0; i < keySize; i++){
        keyAttr = (unsigned short)schema->keyAttrs[i];
        memcpy(curOffset, &keyAttr, sizeof(keyAttr));
        curOffset += sizeof(keyAttr);
    }

    for(int i = 0; i < numAttr; i++){
        memcpy(curOffset, &strLen[i], sizeof(strLen[i]));
        curOffset += sizeof(strLen[i]);
        memcpy(curOffset, &schema->attrNames[i], strLen[i]);
        curOffset += strLen[i];
    }

    free(typeAndLength);
    free(keyAttrs);
    free(strLen);
    return RC_OK;
}

static RC appendToFreeLinkedList(RM_PageFileHeader * pfhr,
                                 RM_PageHeader * phr,
                                 BM_BufferPool * bm)
    {
    RC returnCode = RC_INIT;
    //update nextPage, prevPage
    BM_PageHandle nextPageHandle;
    //this page is the header
    //update the next of page header to previous pageFileheader next
    phr->nextFreePage = pfhr->nextFreePage;
    //update pageFileHeader
    pfhr->nextFreePage = phr->prevFreePage;//current page Number
    //if the list isn't empty
    //update ref of the old head
    if(pfhr->nextFreePage != NO_PAGE){
        //update next page Header
        ASSERT_RC_OK(pinPage(bm, &nextPageHandle, phr->nextFreePage));
        RM_PageHeader * nphr = (RM_PageHeader *) &nextPageHandle.data;
        //update the head
        nphr->prevFreePage = phr->prevFreePage;
        ASSERT_RC_OK(markDirty(bm,&nextPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&nextPageHandle));
    }
    //update the prev of head to no page
    phr->prevFreePage = NO_PAGE;
    return RC_OK;
    }

static RC deleteFromFreeLinkedList(RM_PageFileHeader* pfhr,
                                     RM_PageHeader *phr,
                                     BM_BufferPool*bm)
    {
    RC returnCode = RC_INIT;

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
        RM_PageHeader * pphr = (RM_PageHeader *) &prevPageHandle.data;
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
        RM_PageHeader * nphr = (RM_PageHeader *) &nextPageHandle.data;
        //update the head
        nphr->prevFreePage = phr->prevFreePage;
        ASSERT_RC_OK(markDirty(bm,&nextPageHandle));
        ASSERT_RC_OK(unpinPage(bm,&nextPageHandle));
    }
    //update the current page next and prev
    phr->nextFreePage = NO_PAGE;
    phr->prevFreePage = NO_PAGE;
    return RC_OK;
}

/*********************************************************************
Gets the byte offset value of an attribute in the record for setting
new value to the attribute or for retrieving the value from the record
INPUT:
	*schema: initialized schema - used for locating the attribute in record
	attrNum: The index of the attribute in the record
*********************************************************************/
static int getAttrOffset(Schema *schema, int attrNum){
	int offset = 0;
	for(int i = 0; i < attrNum; i++) {
		offset += schema->typeLength[i];
	}
	return offset;
}
/*********************************************************************
*
*               PAGEFILE HEADER GETTERS AND SETTERS
*
*********************************************************************/
static unsigned short getRecordSizePF(char *pfHdrFrame){
    unsigned short recordSize;
    memcpy(&recordSize, pfHdrFrame + recordSizeOffset, sizeof(unsigned short));
    return recordSize;
}
static void setRecordSizePF(char *pfHdrFrame, unsigned short recordSize){
    memcpy(pfHdrFrame + recordSizeOffset, &recordSize, sizeof(unsigned short));
}
static unsigned int getNumTuplesPF(char *pfHdrFrame){
    unsigned int numTuples;
    memcpy(&numTuples, pfHdrFrame + numTuplesOffset, sizeof(unsigned int));
    return numTuples;
}
static void setNumTuplesPF(char *pfHdrFrame, unsigned int numTuples){
    memcpy(pfHdrFrame + numTuplesOffset, &numTuples, sizeof(unsigned int));
}
static unsigned int getNextFreePage(char *pfHdrFrame){
    unsigned int nextFreePage;
    memcpy(&nextFreePage, pfHdrFrame + nextFreePageOffset, sizeof(unsigned int));
    return nextFreePage;
}
static void setNextFreePage(char *pfHdrFrame, unsigned int nextFreePage){
    memcpy(pfHdrFrame + nextFreePageOffset, &nextFreePage, sizeof(unsigned int));
}
static unsigned short getNumSlotsPerPage(char *pfHdrFrame){
    unsigned short numSlotsPerPage;
    memcpy(&numSlotsPerPage, pfHdrFrame + numSlotsPerPageOffset, sizeof(unsigned short));
    return numSlotsPerPage;
}
static unsigned short getSchemaSize(char *pfHdrFrame){
    unsigned short schemaSize;
    memcpy(&schemaSize, pfHdrFrame + schemaSizeOffset, sizeof(unsigned short));
    return schemaSize;
}
static void getSchema(char *pfHdrFrame, Schema *schema){
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
    for(int i = 0; i < schema->numAttr; i++){
        schema->dataTypes[i] = getIthDataType(pfHdrFrame, i);
        schema->typeLength[i] = getIthTypeLength(pfHdrFrame, i);
        schema->attrNames[i] = getIthAttrName(pfHdrFrame, i);
    }
    for(int i = 0; i < schema->keySize; i++){
        keyAttrs[i] = getIthKeyAttr(pfHdrFrame, i);
    }
}
static unsigned short getNumAttr(char *pfHdrFrame){
    unsigned short numAttr;
    memcpy(&numAttr, pfHdrFrame + numAttrOffset, sizeof(unsigned short));
    return numAttr;
}
static DataType getIthDataType(char *pfHdrFrame, int ithSlot){
    unsigned short dataType;
    memcpy(&dataType, pfHdrFrame + dataTypeOffset(ithSlot), sizeof(unsigned short));
    switch(dataType){
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
static unsigned short getIthTypeLength(char *pfHdrFrame, int ithSlot){
    unsigned short typeLength;
    memcpy(&typeLength, pfHdrFrame + typeLengthOffset(ithSlot), sizeof(unsigned short));
    return typeLength;
}
static unsigned short getKeySize(char *pfHdrFrame){
    unsigned short keySize;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    memcpy(&keySize, pfHdrFrame + keySizeOffset, sizeof(unsigned short));
    return keySize;
}
static unsigned short getIthKeyAttr(char *pfHdrFrame, int ithSlot){
    unsigned short keyAttr;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    memcpy(&keyAttr, pfHdrFrame + keyAttrOffset(ithSlot), sizeof(unsigned short));
    return keyAttr;
}
static unsigned short getIthNameLength(char *pfHdrFrame, int ithSlot){
    unsigned short nameLength;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    unsigned short keySize = getKeySize(pfHdrFrame);
    char *ptr = pfHdrFrame + attrNamesOffset;
    for(int i = 0; i <= ithSlot; i++){
        memcpy(&nameLength, ptr, sizeof(unsigned short));
        ptr = ptr + nameLength + sizeof(unsigned short);
    }
    return nameLength;
}
static char* getIthAttrName(char *pfHdrFrame, int ithSlot){
    unsigned short nameLength;
    unsigned short numAttr = getNumAttr(pfHdrFrame);
    unsigned short keySize = getKeySize(pfHdrFrame);
    char *ptr = pfHdrFrame + attrNamesOffset + sizeof(unsigned short);
    for(int i = 0; i < ithSlot; i++){
        nameLength = getIthNameLength(pfHdrFrame, i);
        ptr = ptr + nameLength + sizeof(unsigned short);
    }
    nameLength = getIthNameLength(pfHdrFrame, ithSlot);
    VALID_CALLOC(char, attrName, 1, nameLength+1); //room for null char
    memcpy(&attrName, ptr, nameLength);
    return attrName;
}
