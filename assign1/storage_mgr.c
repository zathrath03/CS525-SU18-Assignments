#include "storage_mgr.h"
#include "dberror.h"
#include <windows.h>
#include <stdio.h>
#include <math.h>


void initStorageManager() {
    printf("initialization  in progress, please hold\n\n");
    int x;
    for(x = 0; x <10; x++) {
        printf("we are %d %% \n",x*10);
        Sleep(1);
    }
}

RC createPageFile(char *fileName) {
    if(!fileName) {
        return RC_NO_FILENAME;
    }
    FILE * file_ptr = fopen(fileName, "wb+");
    if(!file_ptr) {
        return RC_FILE_CREATION_FAILED;
    }
    int i;
    for(i=0; i<PAGE_SIZE+10; i++) {
        if(fwrite("\0",1,1,file_ptr)<0) {
            return RC_FILE_CREATION_FAILED;
        }
    }
    if(fclose(file_ptr)<0) {
        return RC_FILE_NOT_CLOSED;
    }
    return RC_OK;
}


RC openPageFile(char * fileName, SM_FileHandle *fHandle) {
    if(!fileName) {
        return RC_NO_FILENAME;
    }
    FILE * file_ptr = fopen(fileName, "rb");
    if(!file_ptr) {
        return RC_FILE_NOT_FOUND;
    }
    fHandle->fileName = fileName;
    fHandle->mgmtInfo = file_ptr;
    fHandle->curPagePos = 0;
    //find file number of pages
    //seek file to end
    int prev = ftell(file_ptr);
    if(fseek(file_ptr,0L,SEEK_END)<0) {
        return RC_FILE_NOT_INITIALIZED;
    }
    //get size
    int totalNumPages = ftell(file_ptr);
    //seek back to beginning
    fseek(file_ptr,prev,SEEK_SET);
    totalNumPages  = (totalNumPages /PAGE_SIZE);
    fHandle->totalNumPages = totalNumPages;
    return RC_OK;
}


RC closePageFile(SM_FileHandle* fHandle) {
    FILE* fp = (FILE *)fHandle->mgmtInfo;
    if(!fp) {
        return RC_FILE_HANDLE_NOT_INIT;
    }
    if(fclose(fp)<0) {
        return RC_FILE_NOT_CLOSED;
    }
    free(fHandle);
    fHandle = NULL;
    return RC_OK;
}


RC destroyPageFile (char *fileName) {
    if(!fileName) {
        return RC_NO_FILENAME;
    }
    if(remove(fileName)!=0) {
        return RC_FILE_NOT_FOUND;
    }
    return RC_OK;
}

/***********************************************************
*                                                          *
*              Writing blocks to a page file               *
*                                                          *
***********************************************************/

/***********************************************************
Write a page to disk using absolute position
pageNum: The page in the file referred to by fHandle at
         which the data is to be written. Must be >= 0.
fHandle: Struct which contains the FILE *stream destination
         stored in ->mgmtInfo
memPage: The page in main memory that is to be written to disk.
         memPage is required to be <= PAGE_SIZE
RETURNS: RC_OK, RC_FILE_HANDLE_NOT_INIT, RC_FILE_NOT_INITIALIZED,
         RC_FILE_OFFSET_FAILED, RC_INCOMPATIBLE_BLOCKSIZE,
         or RC_FILE_WRITE_FAILED
*/
RC writeBlock(int pageNum, SM_FileHandle* fHandle, SM_PageHandle memPage) {
    //used if calling a function that returns an RC
    RC returnCode;
    //check that the file handle exists
    if (!fHandle)
        return RC_FILE_HANDLE_NOT_INIT;
    //check that the file the file handle points to exists
    if (!fHandle->mgmtInfo)
        return RC_FILE_NOT_INITIALIZED;
    //negative page numbers are not allowed
    if (pageNum < 0)
        return RC_FILE_OFFSET_FAILED;
    //check that the memPage can fit in a block
    if (sizeof(*memPage) > PAGE_SIZE)
        return RC_INCOMPATIBLE_BLOCKSIZE;
    //expands the file if necessary to write at pageNum
    if ((returnCode = ensureCapacity(pageNum, fHandle)) != RC_OK)
        return returnCode;
    //moves the write pointer to the correct page
    if (fseek(fHandle->mgmtInfo, pageNum*PAGE_SIZE, SEEK_SET) != 0)
        return RC_FILE_OFFSET_FAILED;
    //writes memPage to the file and flushes the stream
    if (fwrite(memPage, PAGE_SIZE, 1, fHandle->mgmtInfo) != 1 || fflush(fHandle -> mgmtInfo) != 0)
        return RC_WRITE_FAILED;

    return RC_OK;
}

/***********************************************************
Write a page to disk relative position
fHandle: Struct which contains the FILE *stream destination
         stored in -> mgmtInfo
memPage: The page in main memory that is to be written to disk.
         memPage is required to be <= PAGE_SIZE
RETURNS: RC_OK, RC_FILE_HANDLE_NOT_INIT, RC_FILE_NOT_INITIALIZED,
         RC_FILE_OFFSET_FAILED, RC_INCOMPATIBLE_BLOCKSIZE,
         or RC_FILE_WRITE_FAILED
*/
RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

/***********************************************************
Increase the number of pages in the file by one. The last
page is filled with null bytes.
fHandle: Struct which contains the FILE *stream destination
RETURNS: RC_OK, RC_FILE_HANDLE_NOT_INIT, RC_FILE_NOT_INITIALIZED,
         RC_FILE_OFFSET_FAILED, or RC_FILE_WRITE_FAILED
*/
RC appendEmptyBlock (SM_FileHandle *fHandle) {
    //check that the file handle exists
    if (!fHandle)
        return RC_FILE_HANDLE_NOT_INIT;
    //check that the file the file handle points to exists
    if (!fHandle->mgmtInfo)
        return RC_FILE_NOT_INITIALIZED;
    /*moves the write pointer to the end
    //offset from SEEK_SET vice SEEK_END since library implementations
    //are allowed to not meaningfully support SEEK_END
    */
    if (fseek(fHandle->mgmtInfo, fHandle->totalNumPages*PAGE_SIZE, SEEK_SET) != 0)
        return RC_FILE_OFFSET_FAILED;
    //creates an array of null elements equal to PAGE_SIZE bytes
    char buffer[PAGE_SIZE] = {0};
    //writes the buffer to the file and flushes the stream
    if (fwrite(buffer, PAGE_SIZE, 1, fHandle->mgmtInfo) != 1 || fflush(fHandle -> mgmtInfo) != 0)
        return RC_WRITE_FAILED;
    //increments the total number of pages in the FileHandle struct
    fHandle->totalNumPages++;
    return RC_OK;
}

/***********************************************************
If the file has fewer than numberOfPages pages, then
increase the size to numberOfPages
fHandle: Struct which contains the FILE *stream destination
RETURNS: RC_OK, RC_FILE_HANDLE_NOT_INIT, RC_FILE_NOT_INITIALIZED,
         RC_FILE_OFFSET_FAILED, RC_INCOMPATIBLE_BLOCKSIZE,
         or RC_FILE_WRITE_FAILED
*/
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle){
    //used if calling a function that returns an RC
    RC returnCode;
    //check that the file handle exists
    if (!fHandle)
        return RC_FILE_HANDLE_NOT_INIT;
    //check that the file the file handle points to exists
    if (!fHandle->mgmtInfo)
        return RC_FILE_NOT_INITIALIZED;
    //increases the number of pages by appending pages
    while (numberOfPages > fHandle->totalNumPages){
        if((returnCode = appendEmptyBlock(fHandle)) != RC_OK)
            return returnCode;
    }
    return RC_OK;
}
