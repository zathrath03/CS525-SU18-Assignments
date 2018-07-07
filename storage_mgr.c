#include "storage_mgr.h"
#include "dberror.h"
#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include <math.h>


void initStorageManager(){
    return;
}

RC createPageFile(char *fileName){
    if(!fileName){
        return RC_NO_FILENAME;
    }
    FILE * file_ptr = fopen(fileName, "wb+");
    if(!file_ptr){
        return RC_FILE_CREATION_FAILED;
    }
    int i;
    for(i=0;i<PAGE_SIZE;i++){
        if(fwrite("\0",1,1,file_ptr)<0){
            return RC_FILE_CREATION_FAILED;
        }
    }
    if(fclose(file_ptr)<0){
        return RC_FILE_NOT_CLOSED;
    }
    return RC_OK;
}


RC openPageFile(char * fileName, SM_FileHandle *fHandle){
    if(!fileName){
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
    if(!fp){
        return RC_FILE_HANDLE_NOT_INIT;
    }
    if(fclose(fp)<0){
        return RC_FILE_NOT_CLOSED;
    }
    //just for safety
    free(fHandle);
    fHandle = NULL;
    return RC_OK;
}


RC destroyPageFile (char *fileName){
    if(!fileName){
        return RC_NO_FILENAME;
    }
    if(remove(fileName)!=0){
        return RC_FILE_NOT_FOUND;
    }
    return RC_OK;
}

