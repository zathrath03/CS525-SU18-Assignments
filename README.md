###Manipulating page files
```c
void initStorageManager (void);
```
Initializes the storage manager? Since it neither takes nore returns any parameters, I am not sure what it actually does.

```c
RC createPageFile (char *fileName);
```
**INPUT**: string with the name of the file  
**RETURN**: integer return code defined in dberror.h  
*  Create a page file with name fileName 
	*  The page file created is one page, with PAGE_SIZE defined in dberror.h. 
	*  use `fopen(fileName, "w");` 
*  Fill the page with nulls (\0) 
*  Return a return code (RC) based on the results 
	*  RC_OK if successful 
	*  Need dberror.h to see the others

``c
RC openPageFile
``