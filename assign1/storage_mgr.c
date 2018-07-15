#include "storage_mgr.h"
#include "dberror.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>

void initStorageManager(){
    return;
}

RC createPageFile(char *fileName){
    if(!*fileName){
        return RC_NO_FILENAME;
    }
    FILE * file_ptr = fopen(fileName, "wb");
    if(!file_ptr){
        return RC_FILE_CREATION_FAILED;
    }
    //creates an array of null elements equal to PAGE_SIZE bytes
    char buffer[PAGE_SIZE] = {0};
    //writes the buffer to the file
    if (fwrite(buffer, PAGE_SIZE, 1, file_ptr) != 1)
        return RC_WRITE_FAILED;
    if(fclose(file_ptr) == EOF){
        return RC_FILE_NOT_CLOSED;
    }
    return RC_OK;
}


RC openPageFile(char * fileName, SM_FileHandle *fHandle){
    if(!*fileName){
        return RC_NO_FILENAME;
    }
    FILE * file_ptr = fopen(fileName, "rb+");
    if(!file_ptr){
        return RC_FILE_NOT_FOUND;
    }
    fHandle->fileName = fileName;
    fHandle->mgmtInfo = file_ptr;
    fHandle->curPagePos = 0;
    //struct to maintain file stats
    struct stat st;
    if(stat(fileName,&st)!=0){
        return RC_FILE_NOT_INITIALIZED;
    }
    int totalNumPages = st.st_size;
    totalNumPages  = ceil(totalNumPages /(double) PAGE_SIZE);
    fHandle->totalNumPages = totalNumPages;
    return RC_OK;
}


RC closePageFile(SM_FileHandle* fHandle){
    FILE* fp = (FILE *)fHandle->mgmtInfo;
    //check that the file handle exists
    if (!(fHandle->fileName) || !(fHandle->totalNumPages))
        return RC_FILE_HANDLE_NOT_INIT;
    //check that the file the file handle points to exists
    if (!fHandle->mgmtInfo)
        return RC_FILE_NOT_INITIALIZED;
    if(fclose(fp)<0){
        return RC_FILE_NOT_CLOSED;
    }
    //just for safety
    free(fHandle);
    fHandle = ((void*)0); //NULL
    return RC_OK;
}


RC destroyPageFile (char *fileName){
    if(!*fileName){
        return RC_NO_FILENAME;
    }
    if(remove(fileName)!=0){
        printf("FILE NOT CLOSED, we will try unlink.\n");
        char filePath[1024];
        if(!getcwd(filePath,sizeof(filePath))){
            printf("Something wrong.\n");
            return RC_FILE_NOT_FOUND;
        }
        strcat(filePath, "\\");
        strcat(filePath, fileName);
        if(unlink(filePath)<0){
            printf("FILE DELETION WILL BE DEFFERED.\n");
        }
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
         Only writes the first PAGE_SIZE bytes to disk
RETURNS: RC_OK, RC_FILE_HANDLE_NOT_INIT, RC_FILE_NOT_INITIALIZED,
         RC_FILE_OFFSET_FAILED, RC_INCOMPATIBLE_BLOCKSIZE,
         or RC_FILE_WRITE_FAILED
*/
RC writeBlock(int pageNum, SM_FileHandle* fHandle, SM_PageHandle memPage) {
    //used if calling a function that returns an RC
    RC returnCode;
    //check that the file handle exists
    if (!(fHandle->fileName) || !(fHandle->totalNumPages))
        return RC_FILE_HANDLE_NOT_INIT;
    //check that the file the file handle points to exists
    if (!fHandle->mgmtInfo)
        return RC_FILE_NOT_INITIALIZED;
    //negative page numbers are not allowed
    if (pageNum < 0)
        return RC_FILE_OFFSET_FAILED;
    //expands the file if necessary to write at pageNum
    if ((returnCode = ensureCapacity(pageNum, fHandle)) != RC_OK)
        return returnCode;
    //moves the write pointer to the correct page
    if (fseek(fHandle->mgmtInfo, pageNum*PAGE_SIZE, SEEK_SET) != 0)
        return RC_FILE_OFFSET_FAILED;
    //update current page position based on fseek
    fHandle->curPagePos = pageNum;
    //writes memPage to the file and flushes the stream
    if (fwrite(memPage, PAGE_SIZE, 1, fHandle->mgmtInfo) != 1 || fflush(fHandle -> mgmtInfo) != 0)
        return RC_WRITE_FAILED;

    return RC_OK;
}

/***********************************************************
Write a page to disk using relative position
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
    if (!(fHandle->fileName) || !(fHandle->totalNumPages))
        return RC_FILE_HANDLE_NOT_INIT;
    //check that the file the file handle points to exists
    if (!fHandle->mgmtInfo) //NULL
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
    //move the write pointer back to the current page
    if (fseek(fHandle->mgmtInfo, fHandle->curPagePos*PAGE_SIZE, SEEK_SET) != 0)
        return RC_FILE_OFFSET_FAILED;

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
    if (!(fHandle->fileName) || !(fHandle->totalNumPages))
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




/*********************************************************
*
*              reading blocks from disc
*
*********************************************************/
//read a block from a file
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){
    //check if file handle exists
    if (!(fHandle->fileName) || !(fHandle->totalNumPages))
        return RC_FILE_HANDLE_NOT_INIT;

    //check if the file handle pointer exists
    if (!fHandle->mgmtInfo)
        return RC_FILE_NOT_INITIALIZED;

    //check if page number is valid
    if(pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_READ_NON_EXISTING_PAGE;

    //moves the write pointer to the correct page
    if (fseek(fHandle->mgmtInfo, pageNum*PAGE_SIZE, SEEK_SET) != 0)
        return RC_FILE_OFFSET_FAILED;

    //read page from disk to memory
    if (fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)
        return RC_READ_FILE_FAILED;

    return RC_OK;
}

//get position of the current block
int getBlockPos (SM_FileHandle *fHandle){
    //check if file handle exists
    if (!(fHandle->fileName) || !(fHandle->totalNumPages))
        return RC_FILE_HANDLE_NOT_INIT;

    return fHandle->curPagePos;
}

//read block with pagenum = 0
//if error reading page, return error code
//else if page read is successful set current page to 0
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    RC returnCode = readBlock(0, fHandle, memPage);
    if(returnCode == RC_OK)
        fHandle->curPagePos = 0;

    return returnCode;
}

//read previous block pagenum = currentpagenum - 1
//if error reading page, return error code
//else if page read is successful set current page to new page number
RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int page = fHandle->curPagePos - 1;
    RC returnCode = readBlock(page, fHandle, memPage);
    if(returnCode == RC_OK)
        fHandle->curPagePos = page;

    return returnCode;
}

//read block with current page number
RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

//read previous block pagenum = currentpagenum + 1
//if error reading page, return error code
//else if page read is successful set current page to new page number
RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int page = fHandle->curPagePos + 1;
    RC returnCode = readBlock(page, fHandle, memPage);
    if(returnCode == RC_OK)
        fHandle->curPagePos = page;

    return returnCode;
}

//read block with pagenum = last page number
//if error reading page, return error code
//else if page read is successful set current page to new page number
RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int page = fHandle->totalNumPages - 1;
    RC returnCode = readBlock(page, fHandle, memPage);
    if(returnCode == RC_OK)
        fHandle->curPagePos = page;

    return returnCode;
}



