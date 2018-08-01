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
