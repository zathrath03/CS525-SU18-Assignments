##Note: Compillation must be performed using the C11 standard

###Manipulating page files
```c
void initStorageManager (void);
```
Initializes the storage manager by doing nothing.


```c
RC createPageFile (char *fileName);
```
Creates a new page file named fileName of size PAGE_SIZE initialized to NULL

**INPUT**: string with the name of the file

**RETURN**: RC_OK, RC_FILE_CREATION_FAILED, RC_FILE_NOT_CLOSED, or RC_NO_FILENAME

* PAGE_SIZE defined in dberror.h.
* Checks for validity of the input fileName string
* Uses `fopen(fileName, "wb");` to create an empty binary file
	* `fopen()` returns either a FILE \*stream or ((void\*)0)
* Fills the page with nulls (\0)
* Closes the file
	* `fclose()` flushes the 
* Returns a return code (RC) based on the results


```c
RC openPageFile (char *fileName, SM_FileHandle *fHandle);
```
**INPUT**: string with the name of the file and an empty FileHandle structure in which to store the file information	
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_NOT_INITIALIZED (9) or RC_NO_FILENAME (14)	
* Opens an existing page file with the name fileName		
	* return RC_FILE_NOT_FOUND if the file does not exist	
	* open with `fopen(fileName, "rb+")` so we can use the same file handle for reading and writing	
* store fileName in fHandle -> fileName	
* count number of pages and store in fHandle -> totalNumPages	
* set `fHandle -> curPagePos  = 0`		
	* The assignment says "When opening a file, the current page should be the first page in the file (curPagePos=0)"	
* set fHandle -> mgmtInfo? I think we should store the FILE pointer here.		
	* The assignment says, "Use the mgmtInfo to store additional information about the file needed by your implementation, e.g., a POSIX file descriptor."	
* Return the correct return code	


```c
RC closePageFile (SM_FileHandle *fHandle);
```
**INPUT**: A file handle struct	
**RETURN**: RC_OK (0) or RC_FILE_HANDLE_NOT_INIT (2) or RC_FILE_NOT_CLOSED (12)	
* Close the page file using `fclose(fHandle -> mgmtInfo)` (assuming we are storing the FILE pointer in mgmtInfo)		
	* fclose() returns 0 if successful	
* Return the correct RC	


```c
RC destroyPageFile (char *fileName);
```
**INPUT**: String with the file name	
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_NOT_CLOSED (12) or RC_NO_FILENAME (14)	
* Delete the file using `remove(fileName)`		
	* returns 0 if successful, otherwise nonzero	
* Return the correct RC	


###Reading blocks from disc
```c
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage);
```
**INPUT**: a page number, a file handle struct, and a struct that points to the area of memory where we are reading the page to	
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_HANDLE_NOT_INIT (2) or RC_READ_NON_EXISTING_PAGE (4) or RC_FILE_NOT_INITIALIZED (9) or RC_FILE_OFFSET_FAILED (10) or RC_FILE_READ_FAILED (11)	
* Check to make sure that the page number is <= fHandle -> totalNumPages	
* offset from (fHandle -> mgmtInfo) by pageNum\*PAGE_SIZE using fseek()	
* Copy that page to memPage either using memcpy() or fread()	
* return the correct RC	


```c
RC int getBlockPos (SM_FileHandle *fHandle);
```
**INPUT**: file handle struct * must be initiated!	
**RETURN**: -1 if the file handle struct isn't initiated, otherwise just `return fHandle -> curPagePos;`	
* Check to make sure curPagePos is valid	
* Return curPagePos or -1	


```c
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
```
* should just be able to call readBlock with 0 for pageNum	
* return the RC passed from readBlock	


```c
RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage);
```
* Get current block from fHandle -> curPagePos	
* Check that a previous block exists (ie that we're not already on the first page)	
* call readBlock with (fHandle -> curPagePos) - 1	
* return the RC passed form readBlock	


###Writing blocks to a page file
```c
RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage);
```
**INPUT**: A file handle with a file pointer stored in mgmtInfo, a destination page number, and a memPage containing data to be written to disk  
**RETURN**: RC_OK (0), RC_FILE_HANDLE_NOT_INIT (2), RC_WRITE_FAILED (3), RC_WRITE_TO_OUTPUTSTREAM_FAILED (7), RC_FILE_NOT_INITIALIZED (9), RC_FILE_OFFSET_FAILED (10), RC_CURRENT_PAGE_NOT_FOUND (13)  
* Writes data from memPage into the file at fHandle -> mgmtInfo at page pageNum  
* Check that the file handle has been initialized  	
  * make sure both that mgmtInfo points to a valid file	
* Check that the fHandle -> totalNumPages is larger than the pageNum (call ensureCapacity())  	
  * call ensureCapacity() which will both check the size and expand if necessary	
* Change the offset to point at the page specified by pageNum using fseek()  	
  * check that the seek succeeded	
* Write the data from the memPage to the file page	
* Use fflush() to empty the write buffer and push the data to disk	
* return the appropriate RC	


```c
RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage);
```
* Call `writeBlock(fHandle->curPagePos, fHandle, memPage);`	
* Return whatever RC is returned by writeBlock()	
	

```c
RC appendEmptyBlock (SM_FileHandle *fHandle);
```
**INPUT**: An initialized file handle	
**RETURN**: RC_OK (0), RC_FILE_NOT_FOUND (1), RC_FILE_HANDLE_NOT_INIT (2), RC_WRITE_FAILED (3), RC_FILE_NOT_INITIALIZED (9), RC_FILE_OFFSET_FAILED (10)  
* Adds a page of size PAGE_SIZE to the end of the file in fHandle -> mgmtInfo  
* Check that the file handle is initialzed and the file exists	
* Move to the end of the file using `fseek (fHandle -> mgmtInfo, fHandle -> totalNumPages * PAGE_SIZE, SEEK_SET)`	
* Write a page worth of '\0'	
* Increment fHandle -> totalNumPages	
* Return appropriate RC	


```c
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle);
```
**INPUT**: a file handle and the number of pages it is desired that the file handle contain	
**RETURN**: RC_OK (0), RC_FILE_NOT_FOUND (1), RC_FILE_HANDLE_NOT_INIT (2), RC_WRITE_FAILED (3), RC_FILE_NOT_INITIALIZED (9), RC_FILE_OFFSET_FAILED (10)	
* Checks if fHandle -> totalNumPages is already greater than the numberOfPages input and returns RC_OK if it is	
* Appends empty blocks onto the end of file referenced by fHandle until fHandle - > totalNumPages == numberOfPages input	
* Passes the return code generated by appendEmptyBlock	