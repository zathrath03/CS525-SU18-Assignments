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
it also initializes the structure for the underlying replacement stragtegy.
#### shutdownBufferPool

```c
RC shutdownBufferPool(BM_BufferPool *const bm);
```
*destroys a buffer pool. This method should free up all resources associated with buffer pool. For example, it should free the memory allocated for page frames. If the buffer pool contains any dirty pages, then these pages should be written back to disk before destroying the pool. It is an error to shutdown a buffer pool that has pinned pages.*
Resources such as:
- pagesFrame ptrs
- struct for poolInfo
- struct for BufferManager

#### forceFlushPool

```c
RC forceFlushPool(BM_BufferPool *const bm);
```
*causes all dirty pages (with fix count 0) from the buffer pool to be written to disk.*

* Initializes a SM_FileHandle in which to store the file data from the storage manager
* Check to see if the memory was correctly allocated, if not then print error code and exit
* Locate a page whose fixCount is 0 and dirtyBit is True.
* Set the address of the beginning of memory page to variable mempage which is of type SM_PageHandle
* Opens the page file specified by bm->pageFile and stores its data in fHandle
* Returns an error return code if openPageFile() does not return RC_OK
* Write the data from that page to the appropriate block on disk using writeBlock()
* Returns an error return code if writeBlock() does not return RC_OK
* Close the page file and return the error return code if closePageFile() does not return RC_OK
* Set the page's dirtyBit to False and increment numWriteIO

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
*marks a page as dirty*

* validates input by verifying that neither the BufferPool nor the PageHandle are null
* uses findFrameNumber() to find which frame has the page pinned in it
* updates the isDirtyArray for the appropriate frame
* returns RC_BM_PAGE_NOT_FOUND if the requested page isn't pinned in a frame

#### forcePage

```c
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page);
```
*writes the current content of the page back to the page file on disk*

* validates input by verifying that neither the BufferPool nor the PageHandle are null
* initializes a SM_FileHandle in which to store the file data from the storage manager
* opens the page file specified by bm->pageFile and stores its data in fHandle
	* returns an error return code if openPageFile() does not return RC_OK
* write the data from the page to the appropriate block on disk using writeBlock()
	* returns an error return code if writeBlock() does not return RC_OK
* close the page file & return the error if closePageFile() does not return RC_OK
* locate the frame with the page pinned in it and reset its dirty flag to false

### Statistics Interface

*These functions return statistics about a buffer pool and its contents. The print debug functions explained below internally use these functions to gather information about a pool.*

#### getFrameContents

```c
PageNumber *getFrameContents (BM_BufferPool *const bm);
```
*returns an array of PageNumbers (of size numPages) where the ith element is the number of the page stored in the ith page frame. An empty page frame is represented using the constant NO_PAGE.*

* Returns the frameContent array

#### getDirtyFlags

```c
bool *getDirtyFlags (BM_BufferPool *const bm);
```
*returns an array of bools (of size numPages) where the ith element is TRUE if the page stored in the ith page frame is dirty. Empty page frames are considered as clean.*

* Returns the isDirtyArray array

#### getFixCounts

```c
int *getFixCounts (BM_BufferPool *const bm);
```
*returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame. Return 0 for empty page frames.*

* Returns the fixCountArray array

#### getNumReadIO

```c
int getNumReadIO (BM_BufferPool *const bm);
```
*returns the number of pages that have been read from disk since a buffer pool has been initialized. You code is responsible to initializing this statistic at pool creating time and update whenever a page is read from the page file into a page frame.*

* Returns numReadIO

#### getNumWriteIO

```c
int getNumWriteIO (BM_BufferPool *const bm);
```
*returns the number of pages written to the page file since the buffer pool has been initialized.*

* Returns numWriteIO

## Page Replacement Strategies

### Clock

*The clock algorithm is a commonly implemented, efficient approximation to LRU. The frames are conceptually arranged in a circle with a hand pointing to one of the frames. The hand rotates clockwise if it needs to find a frame in which to place a disk block. Each frame has an associated flag which is either true or false. When a page is read into a frame or when the contents of a frame are accessed, its flag is set to true. When the buffer manager needs to buffer a new block, it looks for the first 0 it can find, rotating clockwise. If it passes flags that are set to true, it sets them to false. Frames with a false flag are vulnerable to having their contents sent back to disk; frames with a true flag are not. Thus, a page is only thrown out of its frame if it remains unaccessed for the time it takes the hand to make a complete rotation to set its flag to false, and then make another complete roatation to find the frame with its false flag unchanged.*

#### ClockInfo Structure
```c
typedef struct RS_ClockInfo {
    bool *wasReferencedArray;
    int curFrame;
} RS_ClockInfo;
```

RS_ClockInfo maintains an array indexed by frame number that tracks if a frame has been refrenced since last being evaluated for replacement. Also maintains an int corresponding to the frame number that is currently being evaluated for replacement.

#### Clock Replacement Implementation Functions

##### clockInit
```c
void clockInit(BM_BufferPool *bm);
```
Allocates memory and initializes the RS_ClockInfo struct. Does not perform any input validation.

##### clockFree
```c
void clockFree(BM_BufferPool *const bm);
```
Frees memory allocated to the wasReferencedArray and the RS_ClockInfo struct. Does not perform input validation.

##### clockPin
```c
void clockPin(BM_BufferPool *const bm, int frameNum);
```
Sets the flag at index frameNum stored in wasReferencedArray to true to indicate that the frame has been referenced. Does not perform input validation.

##### clockReplace
```c
BM_PageHandle clockReplace (BM_BufferPool *const bm);
```
Implements the replacement strategy. Does not perform input validation.

* Uses curFrame % numPages to loop back to the first index upon reaching the end
* Loops continuously through the fixCountArray and wasReferenced array until it locates one with fixCount == 0 and wasRef == false
* Uses pointer arithmetic to iterate through the arrays
* Returns a pointer to the first frame encountered with fixCount == 0 and wasRef == false
* Sets wasRef to false for every frame encountered
* Will return the first empty frame if not all frames are full since an empty frame will always have fixCount == 0 and wasRef == false
* Would be better implemented with threading
	* There is currently the possiblity for deadlock since threading isn't implemented
	* Could lead to an infinite loop if all the frames are pinned by the same program that is attempting to pin a new page
	* In the case of all frames being pinned (fixCount > 0), clockReplace with a while(true) loops forever
	* Program can't unpin a frame due to clockReplace looking for an unpinned frame

### Least Frequently Used (LFU)

*The LFU algorithm is quite straight forward in its logic.  This algorithms main source of funcitonality is in finding the frequency of each page that is entered into the buffer pool. The implementation of LFU will be done using a linked list and as pages are being pinned into the linked list, we will adjust the frequency as needed. When pinning, we will check for if the page already exists in the buffer pool. If it does then we will increment its frequency, if it does not exist then the alogrithm will add that new page into the linked list after the tail, increment its frequency counter, and re-adjust the tail pointer. Now when it comes to replacing, we will iterate through the linked list until we find the frame number with the lowest frequency. Now we will check if the page we want to replace is the head of the linked list, then we will replace that node and re-adjust the pointers.  If the node we want to replace is pointed by the tail then we will replace it and re-adjust the pointers. If the node is anywhere in between the head and the tail, then we will replace that node and re-adjust the pointers. Finally we return the page handle + frameNum of that replaced page.*

#### RS_LFUInfo Structure
```c
typedef struct RS_LFUInfo {
    struct LFUnode *head;
    struct LFUnode *tail;
} RS_LFUInfo;
```
* RS_LFUInfo consists of a head pointer and a tail pointer which will be used to keep track of the order in which the pages come into the linked list.  These will both be of type LFUnode, which is described below:

### typedef struct LFUnode Structure
```c
typedef struct LFUnode {
    struct LFUnode *nextNode;
    int frequency;
    int frameNumber;
}LFUnode;
```
* LFUnode consists of a pointer to a nodes next node called nextNode.  This will be used when iterating through the linked list of our pages.

#### LFU Replacement Implementation Functions

##### lfuInit
```c
void lfuInit(BM_BufferPool *bm);
```
* Allocates memory for the lfuInfo struct
* Sets the head of lfuInfo to point to NULL
* Sets the tail of lfuInfo to point to NULL

##### lfuFree
```c
void lfuFree(BM_BufferPool *const bm);
```
* Loop through every node in the linked list and free the memory allocated for each node
* Free the structure pointers (head and tail)
* Finally, free the struct itself

##### lfuPin
```c
void lfuPin(BM_BufferPool *const bm, int frameNum);
```
* Loop through linkedlist and check if the page we are pinning to the buffer pool already exists
* If the page already exists, then increment that nodes frequency by 1 and return
* If the page does not already exist, and it is the first page in the linkedlist, then set a head pointer and tail pointer to point to the new node
* If the page does not already exist in the linkedlist, then add it to the tail of the linkedlist and re-adjust the pointers

##### lfuReplace
```c
BM_PageHandle * lfuReplace(BM_BufferPool *const bm);
```
Implements the replacement strategy. Does not perform input validation.

* Create two ints, frequency initialized to the max value of an integer and frameNum initialized to -1, which will be used to iterate through the frequencies and frameNum's of the nodes in our linkedlist
* The node pointer is initialized to point to the head of the linkedlist
* Loop through the nodes in the linkedlist and look for the frame with the lowest value of frequency and which have a fixCount of 0 and store that nodes frameNumber into frameNum and that nodes frequency into the frequency variables
	* If there are no pinned frames in the linkedlist with a fixCount of 0, then return back a void pointer
* Now that we have the frame number for the correct node we want, we will utilize the first in, first out algorithm to find the frame we want
* We will check through the frames:
  * If the first frame in the linkedlist is the frame that we want to replace, then we will remove that frame and re-adjust the pointers. Then return the pageHandle + frameNum.
	* If the head node was not the correct frame, we will iterate through linkedlist to find the correct node, using a while loop
		* Once that correct node is found, then replace that node and re-adjust the pointers. Then return the pageHandle + frameNum.
	* If the node we want to remove is in the tail, then remove the node pointed by tail, and re-adjust the pointers. Then return the pageHandle + frameNum.
	* If the frame is not found in the linkedlist, then return an error saying that the frame was not found and return the pageHandle + frameNum.


## Testing
