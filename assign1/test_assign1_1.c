#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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
static void testErrorCases(void);
static void testReadWrite(void);

/* main function running all tests */
int
main (void) {
    testName = "";

    initStorageManager();

    testCreateOpenClose();
    testSinglePageContent();
    testPageInfo();
    testErrorCases();
    testReadWrite();

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
        //using an if with an error counter to prevent spamming the output with the ASSERT_TRUE messages
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
          testReadWrite will test the certain functions of read and write

          Specifically: READ functions:
                        ---------------
                        ~readBlock----------[done]
                        ~readCurrentBlock---[done]
                        ~readFirstBlock-----[done]
                        ~readLastBlock------[done]
                        ~readNextBlock------[done]
                        ~readPreviousBlock--[done]

                        WRITE functions:
                        ----------------
                        ~writeBlock---------[done]
                        ~writeCurrentBlock--[done]

          Additionally: ~ensureCapacity-----[done]
******************************************************************************/
void
testReadWrite(void) {
    SM_FileHandle fh;
    SM_PageHandle ph;
    int i=0, counter = 0;
    testName = "test READ and WRITE functionality";
    printf("\n\t\tTESTING READ AND WRITE FUNCTIONALITY\n");
    ph = (SM_PageHandle) malloc(PAGE_SIZE);

    // create a new page file
    TEST_CHECK(createPageFile (TESTPF));
    TEST_CHECK(openPageFile (TESTPF, &fh));
    printf("Create and Open new page file");
    //Testing readFirstBlock successful
    TEST_CHECK(readFirstBlock (&fh, ph));
    // the page should be empty (zero bytes)
    for (i=0; i < PAGE_SIZE; i++){
        //using an if with an error counter to prevent spamming the output with the ASSERT_TRUE messages
        if(ph[i] != 0)
            counter ++;
    }
    //Check if first block is empty
    ASSERT_TRUE((counter == 0), "expected zero in every byte of the freshly initialized page");
    printf("first block was empty\n");
    //Check if correct number of pages exist
    ASSERT_TRUE((fh.totalNumPages == 1), "expect 1 page in new file");
    printf("Extend the number of pages to 5");
    //Make sure we have enough pages to work with
    TEST_CHECK(ensureCapacity(5, &fh));
    ASSERT_TRUE((fh.totalNumPages == 5), "Correct amount of pages exist");

    //Writing blocks into file which will be used for reading
    for (i=0; i < PAGE_SIZE; i++)
        ph[i] = '0';
    TEST_CHECK(writeBlock(0, &fh, ph));
    printf("writing into block %d \n", fh.curPagePos);
    //Writing into the next page
    for (i=0; i < PAGE_SIZE; i++)
        ph[i] = '1';
    writeBlock (1, &fh, ph);
    printf("writing into block %d \n", fh.curPagePos);
    //Writing in the third page
    for (i=0; i < PAGE_SIZE; i++)
        ph[i] = '2';
    writeBlock (2, &fh, ph);
    printf("writing into block %d \n", fh.curPagePos);
    //Writing in the fourth page
    for (i=0; i < PAGE_SIZE; i++)
        ph[i] = '3';
    writeBlock (3, &fh, ph);
    printf("writing into block %d \n", fh.curPagePos);
        //Writing in the third page
    for (i=0; i < PAGE_SIZE; i++)
        ph[i] = '4';
    writeBlock (4, &fh, ph);
    printf("writing into block %d \n", fh.curPagePos);
    printf("\n");
    //Test read functions

    //Testing readBlock
    printf("Testing 'readBlock' by reading third block\n");
    TEST_CHECK(readBlock(2,&fh, ph));
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] !='2')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "Read the third block successfully\n");

    //Testing readFirstBlock
    printf("Testing 'readFirstBlock' by reading the first block\n");
    TEST_CHECK(readFirstBlock(&fh, ph));
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] !='0')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "Read the first block successfully\n");

    //Testing readLastBlock
    printf("Testing 'readLastBlock' by reading the last block\n");
    TEST_CHECK(readLastBlock(&fh, ph));
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] !='4')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "Read the last block successfully\n");

    //Testing readCurrentBlock
    fh.curPagePos = 0;
    printf("Testing 'readCurrentBlock' by reading the current block which is set to be block 0\n");
    TEST_CHECK(readCurrentBlock(&fh, ph));
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] !='0')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "Read the current block (first block) successfully\n");

    //Testing readPreviousBlock
    fh.curPagePos = 3;
    printf("Testing 'readPreviousBlock' by reading the previous block which is the 2nd block\n");
    TEST_CHECK(readPreviousBlock(&fh, ph));
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] !='2')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "Read the previous block (block 2) successfully\n");

    //Testing readNextBlock
    printf("Testing 'readNextBlock' by reading the next block which is the 3rd block\n");
    TEST_CHECK(readNextBlock(&fh, ph));
    for (i=0; i < PAGE_SIZE; i++){
        if(ph[i] !='3')
            counter++;
    }
    ASSERT_TRUE((counter == 0), "Read the next block (block 3) successfully\n");


    //close page file before delete
    TEST_CHECK(closePageFile (&fh));
    // destroy new page file
    TEST_CHECK(destroyPageFile (TESTPF));
    TEST_DONE();
}


/*****************************************************************************
          testPageInfo will test the certain steps taken in writeBlock and
          it will test appendEmptyBlock

          Tests:
                  File handle's file initialization----[done]
                  Page size created is correct size----[done]

                  appendEmptyBlock performs correctly--[done]


******************************************************************************/
void
testPageInfo(void) {
    SM_FileHandle fh;
    SM_PageHandle ph;
    printf("\n\t\tTESTING PAGE INFORMATION\n");
    testName = "test page information methods";

    ph = (SM_PageHandle) malloc(PAGE_SIZE);

    TEST_CHECK(createPageFile (TESTPF));
    printf("File created\n");
    TEST_CHECK(openPageFile (TESTPF, &fh));
    printf("File opened\n");
    TEST_CHECK(writeBlock(0, &fh, ph));

    //Check if file handle's file is initialized
    ASSERT_TRUE((fh.mgmtInfo), "File handle's file is initialized");

    //test block size
    struct stat st;
    stat(fh.fileName,&st);
    ASSERT_TRUE((PAGE_SIZE == st.st_size), "Page size is correct");

    //Test appendEmptyBlock by adding page to file
    TEST_CHECK(appendEmptyBlock(&fh));
    ASSERT_EQUALS_INT(2, fh.totalNumPages, "Total page numbers is correct: 2 pages");

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
    testName = ("testErrorCases");
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

    //check RC_FILE_OFFSET_FAILED
    ASSERT_TRUE((writeBlock(-1, &fh, ph) == RC_FILE_OFFSET_FAILED), "writeBlock() RC_FILE_OFFSET_FAILED check");
    int tempCurPag = fh.curPagePos;
    fh.curPagePos = -1;
    ASSERT_TRUE((writeCurrentBlock(&fh, ph) == RC_FILE_OFFSET_FAILED), "writeCurrentBlock() RC_FILE_OFFSET_FAILED check");
    fh.curPagePos = tempCurPag;

    //check RC_INCOMPATIBLE_BLOCKSIZE
    //ph = (SM_PageHandle) malloc(PAGE_SIZE*2);
    //ASSERT_TRUE((writeBlock(0, &fh, ph) == RC_INCOMPATIBLE_BLOCKSIZE), "writeBlock() RC_INCOMPATIBLE_BLOCKSIZE check");

    closePageFile(&fh);
    destroyPageFile(TESTPF);
    free(ph);

    TEST_DONE();
}
