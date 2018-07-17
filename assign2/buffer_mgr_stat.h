/***************************************************
Provides several functions for outputting buffer or
page content to stdout or into a string. The
implementation of these functions is provided so you
do not have to implement them yourself.
***************************************************/

#ifndef BUFFER_MGR_STAT_H
#define BUFFER_MGR_STAT_H

#include "buffer_mgr.h"

/***************************************************
*
*                debug functions
*
***************************************************/
// printPoolContent prints a summary of the current content of a buffer pool
void printPoolContent (BM_BufferPool *const bm);
// printPageContent prints the byte content of a memory page
void printPageContent (BM_PageHandle *const page);
char *sprintPoolContent (BM_BufferPool *const bm);
char *sprintPageContent (BM_PageHandle *const page);

#endif
