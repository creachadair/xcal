/*
    mem.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Memory wrappers for Xcaliber Mark II
 */

#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>

void myfree(void *ptr) {
  fprintf(stderr, "myfree: ptr=0x%p\n", ptr);
  if (ptr != NULL) free(ptr);
}

void *mymalloc(size_t size) {
  void *out = malloc(size);

  fprintf(stderr, "mymalloc: size=%ld out=0x%p\n", size, out);
  return out;
}

void *mycalloc(int num, size_t size) {
  void *out = calloc(num, size);

  fprintf(stderr, "mycalloc: num=%d size=%ld out=0x%p\n", num, size, out);
  return out;
}
#endif
