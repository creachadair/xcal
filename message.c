/*
    message.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Message structure and manipulators for XCaliber Mark II
 */

#include "message.h"

#ifdef DEBUG
#include <stdio.h>
#endif
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#include "mem.h"
#endif

/*------------------------------------------------------------------------*/
mptr msg_alloc(int size) {
  mptr out;

  out = calloc(1, sizeof(message));
#ifdef DEBUG
  fprintf(stderr, "msg_alloc: out=0x%p\n", out);
#endif
  msg_init(out, size);

  return out;
}

/*------------------------------------------------------------------------*/
void msg_init(mptr mp, int size) {
  int ix;

  stream_init(MSTR(mp), size);
  mp->from = -1;
  for (ix = 0; ix < RCPT_LEN; ix++) mp->rcpt[ix] = 0;
  mp->type = M_MSG;
  mp->ref = 1;
}

/*------------------------------------------------------------------------*/
void msg_clear(mptr mp) {
#ifdef DEBUG
  fprintf(stderr, "msg_clear: mp 0x%p\n", mp);
#endif

  if (!mp) return;

  --mp->ref;
  if (mp->ref == 0) {
    int ix;

#ifdef DEBUG
    fprintf(stderr, "msg_clear: refcount of 0x%p became zero\n", mp);
#endif
    stream_clear(MSTR(mp));
    mp->from = -1;
    mp->type = M_MSG;
    for (ix = 0; ix < RCPT_LEN; ix++) mp->rcpt[ix] = 0;
    free(mp);
  }
}

/*------------------------------------------------------------------------*/
mptr msg_copy(mptr mp) {
  if (mp) {
    ++mp->ref;
  }

  return mp;
}

/*------------------------------------------------------------------------*/
void msg_from(mptr mp, int from) { mp->from = from; }

/*------------------------------------------------------------------------*/
void msg_type(mptr mp, byte mt) { mp->type = mt; }

/*------------------------------------------------------------------------*/
void msg_rcpt(mptr mp, int to) {
  if (to == ALL) {
    int ix;

    for (ix = 0; ix < RCPT_LEN; ix++) mp->rcpt[ix] = 0xFF;
  } else {
    mp->rcpt[(to / CHAR_BIT)] |= (1 << (to % CHAR_BIT));
  }
}

/*------------------------------------------------------------------------*/
void msg_unrcpt(mptr mp, int to) {
  mp->rcpt[(to / CHAR_BIT)] &= ~(1 << (to % CHAR_BIT));
}

/*------------------------------------------------------------------------*/
int msg_toall(mptr mp) {
  if (mp->type == M_TOALL)
    return 1;
  else
    return 0;
}

/*------------------------------------------------------------------------*/
int msg_isrcpt(mptr mp, int who) {
  return (mp->rcpt[who / CHAR_BIT] & (1 << (who % CHAR_BIT)));
}

/*------------------------------------------------------------------------*/
int msg_numrcpt(mptr mp) {
  int ix, jx, nr = 0;

  for (ix = 0; ix < RCPT_LEN; ix++) {
    if (mp->rcpt[ix] != 0) {
      for (jx = 0; jx < CHAR_BIT; jx++)
        nr += (mp->rcpt[ix] & (1 << jx)) ? 1 : 0;
    }
  }

  return nr;
}

/*------------------------------------------------------------------------*/
void msg_append(mptr mp, char ch) { stream_putch(MSTR(mp), ch); }

/*------------------------------------------------------------------------*/
void msg_write(mptr mp, char *line) { stream_write(MSTR(mp), line, 0); }
