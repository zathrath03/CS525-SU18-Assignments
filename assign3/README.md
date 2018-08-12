# Assignment 3 - Record Manager

## Definitions
### Page - A PAGE_SIZE chunk of data, regardless of where it is stored
### Block - A storage location for a page on disk
### Frame - A storage location for a page in memory
### PageFile - A file on disk that contains one or more pages
### PageFile Header - The portion of a PageFile that contains metadata for the PageFile
### Page Header - The portion of a page that contains metadata for the page
### Slot - A storage location for a record in a page
### Record - The concatenation of the binary representation of its attributes according to the schema. Not to be confused with the Record struct which also includes the record id.


# Contibutions Break Down:
## Amer Alsabbagh:
// handling records in a table

* extern RC insertRecord (RM_TableData * rel, Record * record);
* extern RC deleteRecord (RM_TableData * rel, RID id);
* extern RC updateRecord (RM_TableData * rel, Record * record);
* extern RC getRecord (RM_TableData * rel, RID id, Record * record);

I also helped with scan functions as well, I set up the underlying page headers bitmap that is used for records handling.  
I helped with testing and debugging other functions as well.
