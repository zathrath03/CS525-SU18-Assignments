#include "storage_mgr.h"
#include "dberror.h"
#include <windows.h>
#include <stdio.h>
#include <math.h>


void initStorageManager(){
    printf("initialization  in progress, please hold\n\n");
    int x;
    for(x = 0; x <10;x++){
        printf("we are %d %% \n",x*10);
        Sleep(1);
    }
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
    for(i=0;i<PAGE_SIZE+10;i++){
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
    FILE * file_ptr = fopen(fileName, "rb");
    if(!file_ptr){
        return RC_FILE_NOT_FOUND;
    }
    fHandle->fileName = fileName;
    fHandle->mgmtInfo = file_ptr;
    fHandle->curPagePos = 0;
    //find file number of pages
    //seek file to end
    int prev = ftell(file_ptr);
    if(fseek(file_ptr,0L,SEEK_END)<0){
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


RC closePageFile(SM_FileHandle* fHandle){
    FILE* fp = (FILE *)fHandle->mgmtInfo;
    if(!fp){
        return RC_FILE_HANDLE_NOT_INIT;
    }
    if(fclose(fp)<0){
        return RC_FILE_NOT_CLOSED;
    }
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

