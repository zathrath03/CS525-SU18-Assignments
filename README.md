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
	- use `fopen(fileName, "wbx");` (or `fopen(fileName, "w+bx");` if we need to be able to read from the file)  
		- using the x mode is a C11 standard that fails if the file already exists and opens the file in exclusive mode  
- Fill the page with nulls (\0)  
- Close the file  
- Return a return code (RC) based on the results  
	- RC_OK if successful  
	- Need dberror.h to see the others  

```c
RC openPageFile (char *fileName, SM_FileHandle *fHandle);
```
**INPUT**: string with the name of the file and an empty FileHandle structure in which to store the file information  
**RETURN**: RC_OK (0) or RC_FILE_NOT_FOUND (1) or RC_FILE_NOT_INITIALIZED (9) or RC_NO_FILENAME (14)
- Opens an existing page file with the name fileName  
	- return RC_FILE_NOT_FOUND if the file does not exist  
- store fileName in fHandle -> fileName  
- count number of pages and store in fHandle ->totalNumPages  
- set fHandle -> curPagePos  
	- Should this be the start or end of the page?  
- set fHandle -> mgmtInfo? Not sure what this portion of the struct is supposed to be used for. I think we should store the FILE pointer here.  
- Return the correct return code

```c
RC closePageFile (SM_FileHandle *fHandle);
```
**INPUT**: A file handle struct  
**RETURN**: RC_OK (0) or RC_FILE_HANDLE_NOT_INIT (2) or RC_FILE_NOT_CLOSED (12)
- Close the page file using `fclose(fHandle -> mgmtInfo)` (assuming we're storing the FILE pointer in mgmtInfo)
- Return the correct RC