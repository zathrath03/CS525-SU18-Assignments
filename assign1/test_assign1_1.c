#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage_mgr.h"
#include "dberror.h"
#include "test_helper.h"

// test name
char *testName;

/* test output files */
#define TESTPF "test_pagefile.bin"

/* prototypes for test functions */
static void testCreateOpenClose(void);
static void testSinglePageContent(void);
static void testPageInfo(void);
static void testEnsureCapacity(void);
static void testAppendEmptyBlock(void);
static void testErrorCases(void);

/* main function running all tests */
int
main (void) {
    testName = "";

    initStorageManager();

    testCreateOpenClose();
    testSinglePageContent();
    testPageInfo();
    testEnsureCapacity();
    testAppendEmptyBlock();
    testErrorCases();

    return 0;
}
//Test the following:
/*
creating a page     (done with all)
  file name
  number of pages
  current page position
closing a page      (done)
destroying a page   (done)

test if filehandle points to correct file (done)
test block size      (not entirely sure if correct)
test appendEmpty
test ensureCapacity   (in progress)

test if info written correctly to disk using "read from disk commands"
*/

/* check a return code. If it is not RC_OK then output a message, error description, and exit */
/* Try to create, open, and close a page file */
void
testCreateOpenClose(void) {
    SM_FileHandle fh;

    testName = "test create open and close methods";

    TEST_CHECK(createPageFile (TESTPF));

    TEST_CHECK(openPageFile (TESTPF, &fh));
    ASSERT_TRUE(strcmp(fh.fileName, TESTPF) == 0, "filename correct");
    ASSERT_TRUE((fh.totalNumPages == 1), "expect 1 page in new file");
    ASSERT_TRUE((fh.curPagePos == 0), "freshly opened file's page position should be 0");

    TEST_CHECK(closePageFile (&fh));
    TEST_CHECK(destroyPageFile (TESTPF));

    // after destruction trying to open the file should cause an error
    ASSERT_TRUE((openPageFile(TESTPF, &fh) != RC_OK), "opening non-existing file should return an error.");

    TEST_DONE();
}

/* Try to create, open, and close a page file */
void
testSinglePageContent(void) {
    SM_FileHandle fh;
    SM_PageHandle ph;
    int i, counter = 0;

    testName = "test single page content";

    ph = (SM_PageHandle) malloc(PAGE_SIZE);

    // create a new page file
    TEST_CHECK(createPageFile (TESTPF));
    TEST_CHECK(openPageFile (TESTPF, &fh));
    printf("created and opened file\n");

    // read first page into handle
    TEST_CHECK(readFirstBlock (&fh, ph));
    // the page should be empty (zero bytes)
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] != 0)
            counter ++;
    }
    ASSERT_TRUE((counter == 0), "expected zero in every byte of the freshly initialized page");
    printf("first block was empty\n");

    // change ph to be a string and write that one to disk
    for (i=0; i < PAGE_SIZE; i++)
        ph[i] = (i % 10) + '0';
    TEST_CHECK(writeBlock (0, &fh, ph));
    printf("writing first block\n");

    // read back the page containing the string and check that it is correct
    TEST_CHECK(readFirstBlock (&fh, ph));
    counter = 0;
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] != (i % 10) + '0')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "character in page read from disk is the one we expected.");
    printf("reading first block\n");

    //close page file before delete
    TEST_CHECK(closePageFile (&fh));
    // destroy new page file
    TEST_CHECK(destroyPageFile (TESTPF));

    TEST_DONE();
}
/*****************************************************************************
          testPageInfo will test the certain steps taken in writeBlock
******************************************************************************/
void
testPageInfo(void) {
    SM_FileHandle fh;
    SM_PageHandle ph;
    testName = "test page information methods";

    ph = (SM_PageHandle) malloc(PAGE_SIZE);

    TEST_CHECK(createPageFile (TESTPF));
    TEST_CHECK(openPageFile (TESTPF, &fh));
    TEST_CHECK(writeBlock(0, &fh, ph));

    //test mgmtInfo
    ASSERT_TRUE((fh.mgmtInfo), "File handle's file is initialized");

    //test block size
    ASSERT_ERROR((sizeof(PAGE_SIZE) - sizeof(ph) >= 0), "Page Handlers block size will fit in a standard page size");

    /* //check that the memPage can fit in a block
       if (sizeof(*memPage) > PAGE_SIZE)
           return RC_INCOMPATIBLE_BLOCKSIZE;
    */
    TEST_CHECK(closePageFile (&fh));
    // destroy new page file
    TEST_CHECK(destroyPageFile (TESTPF));

    TEST_DONE();
}

/*****************************************************************************
    testEnsureCapacity will test the ensureCapacity function
******************************************************************************/
void
testEnsureCapacity(void) {
    SM_FileHandle fh;
    //SM_PageHandle ph;
    testName = "test ensureCapacity method";

    TEST_CHECK(createPageFile (TESTPF));
    TEST_CHECK(openPageFile (TESTPF, &fh));
    TEST_CHECK(ensureCapacity(4,&fh));
    //Check to see if the file handle contains the correct number of pages
    ASSERT_EQUALS_INT(fh.totalNumPages, 4, "Correct number of pages exist");
    // close new page file
    TEST_CHECK(closePageFile (&fh));
    // destroy new page file
    TEST_CHECK(destroyPageFile (TESTPF));

    TEST_DONE();
}

/*****************************************************************************
    testAppendEmptyBlock will test the ensureCapacity function
******************************************************************************/
void
testAppendEmptyBlock(void) {
    SM_FileHandle fh;
    //SM_PageHandle ph;
    testName = "test ensureCapacity method";

    TEST_CHECK(createPageFile (TESTPF));
    TEST_CHECK(openPageFile (TESTPF, &fh));

    //append a page to the new page file
    TEST_CHECK(appendEmptyBlock(&fh));

    //check if a new page has been appended
    ASSERT_EQUALS_INT(fh.totalNumPages, 2, "A second page has been appended");

    //check if the new page is empty with null values using READ COMMANDS


    // close new page file
    TEST_CHECK(closePageFile (&fh));
    // destroy new page file
    TEST_CHECK(destroyPageFile (TESTPF));

    TEST_DONE();
}

void
testErrorCases(void) {
    SM_FileHandle fh;
    SM_PageHandle ph = (SM_PageHandle) malloc(PAGE_SIZE);
    printf("\n\t\tTESTING FUNCTION ARGUMENT ERRORS\n");

    //check RC_NO_FILENAME on invalid fileName
    ASSERT_TRUE((createPageFile("") == RC_NO_FILENAME), "createPageFile() RC_NO_FILENAME check");
    ASSERT_TRUE((openPageFile("", &fh) == RC_NO_FILENAME), "openPageFile() RC_NO_FILENAME check");
    ASSERT_TRUE((destroyPageFile("") == RC_NO_FILENAME), "openPageFile() RC_NO_FILENAME check");

    //check RC_FILE_HANDLE_NOT_INIT on invalid file handle
    ASSERT_TRUE((closePageFile(&fh) == RC_FILE_HANDLE_NOT_INIT), "closePageFile() RC_FILE_HANDLE_NOT_INIT check");
    ASSERT_TRUE((writeBlock(0, &fh, ph) == RC_FILE_HANDLE_NOT_INIT), "writeBlock() RC_FILE_HANDLE_NOT_INIT check");
    ASSERT_TRUE((writeCurrentBlock(&fh, ph) == RC_FILE_HANDLE_NOT_INIT), "writeCurrentBlock() RC_FILE_HANDLE_NOT_INIT check");
    ASSERT_TRUE((appendEmptyBlock(&fh) == RC_FILE_HANDLE_NOT_INIT), "appendEmptyBlock() RC_FILE_HANDLE_NOT_INIT check");
    ASSERT_TRUE((ensureCapacity(0, &fh) == RC_FILE_HANDLE_NOT_INIT), "ensureCapacity() RC_FILE_HANDLE_NOT_INIT check");
    ASSERT_TRUE((readBlock(0, &fh, ph) == RC_FILE_HANDLE_NOT_INIT), "readBlock() RC_FILE_HANDLE_NOT_INIT check");
    ASSERT_TRUE((getBlockPos(&fh) == RC_FILE_HANDLE_NOT_INIT), "getBlockPos() RC_FILE_HANDLE_NOT_INIT check");

    //check RC_FILE_NOT_INITIALIZED
    createPageFile(TESTPF);
    openPageFile(TESTPF, &fh);
    FILE *temp = fh.mgmtInfo; //store the FILE *stream to be able to restore it later
    fh.mgmtInfo = ((void*)0); //NULL

    ASSERT_TRUE((closePageFile(&fh) == RC_FILE_NOT_INITIALIZED), "closePageFile() RC_FILE_NOT_INITIALIZED check");
    ASSERT_TRUE((writeBlock(0, &fh, ph) == RC_FILE_NOT_INITIALIZED), "writeBlock() RC_FILE_NOT_INITIALIZED check");
    ASSERT_TRUE((writeCurrentBlock(&fh, ph) == RC_FILE_NOT_INITIALIZED), "writeCurrentBlock() RC_FILE_NOT_INITIALIZED check");
    ASSERT_TRUE((appendEmptyBlock(&fh) == RC_FILE_NOT_INITIALIZED), "appendEmptyBlock() RC_FILE_NOT_INITIALIZED check");
    ASSERT_TRUE((ensureCapacity(0, &fh) == RC_FILE_NOT_INITIALIZED), "ensureCapacity() RC_FILE_NOT_INITIALIZED check");
    ASSERT_TRUE((readBlock(0, &fh, ph) == RC_FILE_NOT_INITIALIZED), "readBlock() RC_FILE_NOT_INITIALIZED check");

    fh.mgmtInfo = temp; //restores the original FILE *stream

    //check RC_FILE_NOT_CLOSED
    ASSERT_TRUE((destroyPageFile(TESTPF) == RC_FILE_NOT_CLOSED), "destroyPageFile() RC_FILE_NOT_CLOSED check");

    closePageFile(&fh);
    destroyPageFile(TESTPF);

    TEST_DONE();
}
