?? Is there a difference between block and page? They seem to be used interchangably in the problem description.

###Self-generated functions
We should make some functions that check frequently looked for error conditions such as RC_FILE_NOT_FOUND, RC_FILE_HANDLE_NOT_INIT, RC_FILE_NOT_INITIALIZED, RC_NO_FILENAME that we are going to check for frequently so we do not need to duplicate the code each in each of the below functions where we need to check for those conditions.

###Manipulating page files
####Note: Compillation must be performed using the C11 standard
```c
void initStorageManager (void);
```
Initializes the storage manager? Since it neither takes nor returns any parameters, I am not sure what it actually does.
  
```c
RC createPageFile (char *fileName);
```
**INPUT**: string with the name of the file  
**RETURN**: RC_OK (0) or RC_FILE_CREATION_FAILED (6) or RC_NO_FILENAME (14)
- Create a page file with name fileName  
	- The page file created is one page, with PAGE_SIZE defined in dberror.h.  
	- use `fopen(fileName, "wbx");` or `fopen(fileName, "w+bx");` (do we need to be able to read from the file?)  
		- using the x mode is a C11 standard that fails if the file already exists  
- Fill the page with nulls (\0)  
- Close the file  
- Return a return code (RC) based on the results    
  
```c
RC openPageFile (char *fileName, SM_FileHandle *fHandle);
```
**INPUT**: string with the name of the file and an empty FileHandle structure in which to store the file information  
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_NOT_INITIALIZED (9) or RC_NO_FILENAME (14)
- Opens an existing page file with the name fileName  
	- return RC_FILE_NOT_FOUND if the file does not exist  
- store fileName in fHandle -> fileName  
- count number of pages and store in fHandle ->totalNumPages  
- set `fHandle -> curPagePos  = 0`  
	- The assignment says "When opening a file, the current page should be the first page in the file (curPagePos=0)"  
- set fHandle -> mgmtInfo? I think we should store the FILE pointer here.  
	- The assignment says, "Use the mgmtInfo to store additional information about the file needed by your implementation, e.g., a POSIX file descriptor."  
- Return the correct return code  
  
```c
RC closePageFile (SM_FileHandle *fHandle);
```
**INPUT**: A file handle struct  
**RETURN**: RC_OK (0) or RC_FILE_HANDLE_NOT_INIT (2) or RC_FILE_NOT_CLOSED (12)  
- Close the page file using `fclose(fHandle -> mgmtInfo)` (assuming we are storing the FILE pointer in mgmtInfo)  
	- fclose() returns 0 if successful  
- Return the correct RC  
  
```c
RC destroyPageFile (char *fileName);
```
**INPUT**: String with the file name  
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_NOT_CLOSED (12) or RC_NO_FILENAME (14)  
- Delete the file using `remove(fileName)`  
	- returns 0 if successful, otherwise nonzero
- Return the correct RC

###Reading blocks from disc
```c
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage);
```
**INPUT**: a page number, a file handle struct, and a struct that points to the area of memory where we are reading the page to  
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_HANDLE_NOT_INIT (2) or RC_READ_NON_EXISTING_PAGE (4) or RC_FILE_NOT_INITIALIZED (9) or RC_FILE_OFFSET_FAILED (10) or RC_FILE_READ_FAILED (11)  
- Check to make sure that the page number is <= fHandle -> totalNumPages  
- offset from (fHandle -> mgmtInfo) by pageNum\*PAGE_SIZE using fseek()  
- Copy that page to memPage either using memcpy() or fread()  
- return the correct RC  
  
```c
RC int getBlockPos (SM_FileHandle *fHandle);
```
**INPUT**: file handle struct - must be initiated!  
**RETURN**: -1 if the file handle struct isn't initiated, otherwise just `return fHandle -> curPagePos;`  
- Check to make sure curPagePos is valid  
- Return curPagePos or -1  
  
```c
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
```
- should just be able to call readBlock with 0 for pageNum  
- return the RC passed from readBlock  

```c
extern RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage);
```
- Get current block from fHandle -> curPagePos
- Check that a previous block exists (ie that we're not already on the first page)
- call readBlock with (fHandle -> curPagePos) - 1
- return the RC passed form readBlock