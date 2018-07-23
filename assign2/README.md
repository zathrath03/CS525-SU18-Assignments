# CS 525 Assignment 2 - Buffer Manager

## Interface

### Data Structures

#### BM_BufferPool

```c
typedef struct BM_BufferPool {
  char *pageFile;
  int numPages;
  ReplacementStrategy strategy;
  BM_PoolInfo *mgmtData;
} BM_BufferPool;
```
*stores information about a buffer pool: the name of the page file associated with the buffer pool (pageFile), the size of the buffer pool, i.e., the number of page frames (numPages), the page replacement strategy (strategy), and a pointer to bookkeeping data (mgmtData). Similar to the first assignment, you can use the mgmtData to store any necessary information about a buffer pool that you need to implement the interface. For example, this could include a pointer to the area in memory that stores the page frames or data structures needed by the page replacement strategy to make replacement decisions.*

* Using mgmtData to point to a `BM_PoolInfo` struct, defined below
* Changed the type of `mgmtData` to BM_PoolInfo to eliminate the need to constantly typecast it

#### BM_PageHandle

```c
typedef struct BM_PageHandle {
  PageNumber pageNum;
  char *data;
} BM_PageHandle;
```
*stores information about a page. The page number (position of the page in the page file) is stored in pageNum. The page number of the first data page in a page file is 0. The data field points to the area in memory storing the content of the page. This will usually be a page frame from your buffer pool.*

### Buffer Manager Interface Pool Handling

*These functions are used to create a buffer pool for an existing page file (initBufferPool), shutdown a buffer pool and free up all associated resources (shutdownBufferPool), and to force the buffer manager to write all dirty pages to disk (forceFlushPool)*

#### initBufferPool

```c
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData);
```
*creates a new buffer pool with* numPages *page frames using the page replacement strategy* strategy*. The pool is used to cache pages from the page file with name pageFileName. Initially, all page frames should be empty. The page file should already exist, i.e., this method should not generate a new page file. stratData can be used to pass parameters for the page replacement strategy. For example, for LRU-k this could be the parameter k.*

#### shutdownBufferPool

```c
RC shutdownBufferPool(BM_BufferPool *const bm);
```
*destroys a buffer pool. This method should free up all resources associated with buffer pool. For example, it should free the memory allocated for page frames. If the buffer pool contains any dirty pages, then these pages should be written back to disk before destroying the pool. It is an error to shutdown a buffer pool that has pinned pages.*

#### forceFlushPool

```c
RC forceFlushPool(BM_BufferPool *const bm);
```
*causes all dirty pages (with fix count 0) from the buffer pool to be written to disk.*

### Buffer Manager Interface Access Pages

*These functions are used pin pages, unpin pages, mark pages as dirty, and force a page back to disk.*

#### pinPage

```c
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum);
```
*pins the page with page number* pageNum*. The buffer manager is responsible to set the pageNum field of the page handle passed to the method. Similarly, the data field should point to the page frame the page is stored in (the area in memory storing the content of the page).*

#### unpinPage

```c
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page);
```
*unpins the page* page*. The pageNum field of page should be used to figure out which page to unpin.*

#### markDirty

```c
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page);
```
*marks a page as dirty.*

#### forcePage

```c
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page);
```
*should write the current content of the page back to the page file on disk.*

### Statistics Interface

*These functions return statistics about a buffer pool and its contents. The print debug functions explained below internally use these functions to gather information about a pool.*

#### getFrameContents

```c
PageNumber *getFrameContents (BM_BufferPool *const bm);
```
*returns an array of PageNumbers (of size numPages) where the ith element is the number of the page stored in the ith page frame. An empty page frame is represented using the constant NO_PAGE.*

#### getDirtyFlags

```c
bool *getDirtyFlags (BM_BufferPool *const bm);
```
*returns an array of bools (of size numPages) where the ith element is TRUE if the page stored in the ith page frame is dirty. Empty page frames are considered as clean.*

#### getFixCounts

```c
int *getFixCounts (BM_BufferPool *const bm);
```
*returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame. Return 0 for empty page frames.*

#### getNumReadIO

```c
int getNumReadIO (BM_BufferPool *const bm);
```
*returns the number of pages that have been read from disk since a buffer pool has been initialized. You code is responsible to initializing this statistic at pool creating time and update whenever a page is read from the page file into a page frame.*

#### getNumWriteIO

```c
int getNumWriteIO (BM_BufferPool *const bm);
```
*returns the number of pages written to the page file since the buffer pool has been initialized.*

## Page Replacement Strategies

### Clock

```c
BM_PageHandle Clock (BM_BufferPool *const bm);
```

*traverses through the frames*

## Testing