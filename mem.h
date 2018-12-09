/*
    mem.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Memory wrappers for Xcaliber Mark II
 */

#ifndef XCAL_MEM_H_
#define XCAL_MEM_H_

#ifdef DEBUG2
#define free(X)     myfree(X)
#define malloc(X)   mymalloc(X)
#define calloc(X,Y) mycalloc((X),(Y))

extern void myfree(void *ptr);
extern void *mymalloc(size_t size);
extern void *mycalloc(int num, size_t size);

#endif

#endif /* end XCAL_MEM_H_ */
