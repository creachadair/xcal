/*
    stream.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    A bytestream data structure
 */

#include "stream.h"
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

#include "default.h"

#ifndef DBL_DIG
#define DBL_DIG DEFAULT_DBLDIG
#endif

static char s_numbuf[64];

static void s_putint(stream *sp, int v, int prec, char pad);
static void s_putlng(stream *sp, long v, int prec, char pad);
static void s_putstr(stream *sp, char *s, int prec, char pad);
static void s_puthex(stream *sp, int v, int cap);
static void s_putlhx(stream *sp, long v, int cap);
static void s_putdbl(stream *sp, double dval, int prec);
static char s_hexval(int ch, int cap);
static char *s_cvt_num(long val, int minwidth, char pad);

/*------------------------------------------------------------------------*/
void stream_init(stream *sp, int size) {
  sp->data = calloc(size, sizeof(byte));
  sp->size = size;
  stream_flush(sp);
}

/*------------------------------------------------------------------------*/
void stream_clear(stream *sp) {
  stream_flush(sp);
  if (sp->data) free(sp->data);
  sp->data = NULL;
}

/*------------------------------------------------------------------------*/
void stream_grow(stream *sp, int size) {
  byte *tmp;
  int pos = 0;

  if (size > sp->size) {
    tmp = calloc(size, sizeof(byte));

    do {
      tmp[pos++] = sp->data[sp->tail++];
      if (sp->tail >= sp->size) sp->tail = 0;
    } while (sp->tail != sp->head);

    sp->tail = 0;
    sp->head = pos;

    free(sp->data);
    sp->data = tmp;
    sp->size = size;
  }
}

/*------------------------------------------------------------------------*/
void stream_shrink(stream *sp) {
#ifdef DEBUG
  fprintf(stderr, "stream_shrink: size=%d, num=%d\n", sp->size, sp->num);
#endif
  if (sp->size > sp->num) {
    byte *tmp = calloc(sp->num + 1, sizeof(byte));
    int pos = 0;

    while (sp->tail != sp->head) {
      tmp[pos++] = sp->data[sp->tail++];
      if (sp->tail >= sp->size) sp->tail = 0;
    }

    sp->tail = 0;
    sp->head = pos;

    free(sp->data);
    sp->data = tmp;
    sp->size = sp->num;
  }
}

/*------------------------------------------------------------------------*/
int stream_empty(stream *sp) { return (sp->num <= 0); }

/*------------------------------------------------------------------------*/
int stream_full(stream *sp) { return (sp->num >= sp->size); }

/*------------------------------------------------------------------------*/
int stream_length(stream *sp) { return sp->num; }

/*------------------------------------------------------------------------*/
int stream_hasline(stream *sp) {
  int ix, count = 0;

  if (stream_empty(sp)) return 0;

  ix = sp->tail;
  while (ix != sp->head) {
    ++count;

    if (sp->data[ix] == '\r' || sp->data[ix] == '\n') return count;

    ++ix;
    if (ix >= sp->size) ix = 0;
  }

  return 0;
}

/*------------------------------------------------------------------------*/
int stream_getch(stream *sp, byte *b) {
  if (stream_empty(sp)) return 0;

  *b = sp->data[sp->tail++];
  if (sp->tail >= sp->size) sp->tail = 0;
  --sp->num;
  return 1;
}

/*------------------------------------------------------------------------*/
int stream_peekch(stream *sp, byte *b) {
  if (stream_empty(sp)) return 0;

  *b = sp->data[sp->tail];
  return 1;
}

/*------------------------------------------------------------------------*/
void stream_putch(stream *sp, byte b) {
  if (stream_full(sp)) stream_grow(sp, 2 * sp->size);

  sp->data[sp->head++] = b;
  if (sp->head >= sp->size) sp->head = 0;
  ++sp->num;
}

/*------------------------------------------------------------------------*/
int stream_read(stream *sp, byte *b, int len) {
  byte tmp;
  int pos = 0;

  while (pos < len && stream_getch(sp, &tmp)) b[pos++] = tmp;

  return pos;
}

/*------------------------------------------------------------------------*/
byte *stream_readln(stream *sp) {
  int len;
  byte *out, ch, nx;

  if ((len = stream_hasline(sp)) == 0) return NULL;

  out = calloc((len + 1), sizeof(byte));
  stream_read(sp, out, len);
#ifdef DEBUG
  fprintf(stderr, "stream_readln: out=0x%p '%s'\n", out, out);
#endif
  ch = (out[len - 1] == '\n') ? '\r' : '\n';
  out[len - 1] = '\0';

  if (stream_getch(sp, &nx)) {
    if (nx != ch) stream_ungetch(sp, nx);
  }

  return out;
}

/*------------------------------------------------------------------------*/
void stream_write(stream *sp, byte *b, int len) {
  int ix;

  if (len == 0) len = strlen(b);

  for (ix = 0; ix < len; ix++) stream_putch(sp, b[ix]);
}

/*------------------------------------------------------------------------*/
void stream_force(stream *sp, byte *b, int len) {
  int ix;

  if (len == 0) len = strlen(b);

  for (ix = len - 1; ix >= 0; ix--) stream_ungetch(sp, b[ix]);
}

/*------------------------------------------------------------------------*/
int stream_getd(stream *sp, double *d) {
  char *st, *en;
  byte ign;
  int nread;
  double out;

  if (stream_empty(sp)) return 0;

  nread = stream_length(sp);
  st = malloc(nread * sizeof(byte));
  stream_extract(sp, st);

  out = strtod(st, &en);
  nread = (int)(en - st);
  free(st);

  while (nread--) stream_getch(sp, &ign);

  *d = out;
  return 1;
}

/*------------------------------------------------------------------------*/
void stream_ungetch(stream *sp, byte b) {
  if (stream_full(sp)) stream_grow(sp, 2 * sp->size);

  sp->tail = (sp->tail - 1);
  if (sp->tail < 0) sp->tail = sp->size - 1;
  sp->data[sp->tail] = b;
  ++sp->num;
}

/*------------------------------------------------------------------------*/
void stream_flush(stream *sp) {
  sp->head = 0;
  sp->tail = 0;
  sp->num = 0;
}

/*------------------------------------------------------------------------*/
void stream_printf(stream *sp, char *fmt, ...) {
  va_list ap;
  char *sval, pad = ' ';
  int ival, ix, prec = 0, lv = 0, sgn = 1, cval;
  long lval;
  double dval;

  va_start(ap, fmt);

  for (ix = 0; fmt[ix] != '\0'; ix++) {
    if (fmt[ix] != '%')
      stream_putch(sp, fmt[ix]);
    else {
      prec = 0;
      if (fmt[++ix] == '-') {
        sgn = -1;
        ++ix;
      } else if (fmt[ix] == '0') {
        pad = '0';
        ++ix;
      }
      while (isdigit(fmt[ix])) {
        prec = (prec * 10) + (fmt[ix] - '0');
        ++ix;
      }
      if (fmt[ix] == 'l') {
        lv = 1;
        ++ix;
      }
      switch (fmt[ix]) {
        case 'c':
          cval = va_arg(ap, int);
          stream_putch(sp, cval);
          break;

        case 'd':
          if (lv) {
            lval = va_arg(ap, long);
            s_putlng(sp, lval, prec * sgn, pad);
          } else {
            ival = va_arg(ap, int);
            s_putint(sp, ival, prec * sgn, pad);
          }
          break;

        case 'u':
          if (lv) {
            lval = va_arg(ap, long);
            s_putlng(sp, labs(lval), prec * sgn, pad);
          } else {
            ival = va_arg(ap, int);
            s_putint(sp, labs(ival), prec * sgn, pad);
          }
          break;

        case 'f':
          dval = va_arg(ap, double);
          s_putdbl(sp, dval, prec);
          break;

        case 's':
          sval = va_arg(ap, char *);
          s_putstr(sp, sval, prec * sgn, pad);
          break;

        case 'x':
          if (lv) {
            lval = va_arg(ap, long);
            s_putlhx(sp, lval, 0);
          } else {
            ival = va_arg(ap, int);
            s_puthex(sp, ival, 0);
          }
          break;

        case 'X':
          if (lv) {
            lval = va_arg(ap, long);
            s_putlhx(sp, lval, 1);
          } else {
            ival = va_arg(ap, int);
            s_puthex(sp, ival, 1);
          }
          break;

        default:
          stream_putch(sp, fmt[ix]);
          break;
      }
    }
  }

  va_end(ap);
}

/*------------------------------------------------------------------------*/
byte *stream_copy(stream *sp) {
  byte *out;

  out = calloc(sp->num + 1, sizeof(byte));

  if (sp->num == 0)
    out[0] = '\0';
  else
    stream_extract(sp, out);

  return out;
}

/*------------------------------------------------------------------------*/
void stream_extract(stream *sp, byte *buf) {
  int ix = sp->tail, pos = 0;

  do {
    buf[pos++] = sp->data[ix++];
    if (ix >= sp->size) ix = 0;
  } while (ix != sp->head);

  buf[pos] = '\0';
}

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
static void s_putint(stream *sp, int v, int prec, char pad) {
  char *res;

  res = s_cvt_num((long)v, prec, pad);
  stream_write(sp, res, strlen(res));
}

/*------------------------------------------------------------------------*/
static void s_putlng(stream *sp, long v, int prec, char pad) {
  char *res;

  res = s_cvt_num(v, prec, pad);
  stream_write(sp, res, strlen(res));
}

/*------------------------------------------------------------------------*/
static void s_putstr(stream *sp, char *s, int prec, char pad) {
  int ix = 0;
  int sig = 1;

  if (prec < 0) {
    sig = -1;
    prec = -prec;
  }
  while (s[ix] != '\0') {
    stream_putch(sp, s[ix]);
    ++ix;
  }

  while (ix < prec) {
    stream_putch(sp, ' ');
    ++ix;
  }
}

/*------------------------------------------------------------------------*/
static void s_puthex(stream *sp, int v, int cap) {
  int ix, reg, ch;

  for (ix = sizeof(int) - 1; ix >= 0; ix--) {
    reg = (v >> (CHAR_BIT * ix)) & 0xFF;
    ch = (reg >> 4) & 0x0F;
    stream_putch(sp, s_hexval(ch, cap));
    ch = reg & 0x0F;
    stream_putch(sp, s_hexval(ch, cap));
  }
}

/*------------------------------------------------------------------------*/
static void s_putlhx(stream *sp, long v, int cap) {
  int ix, reg, ch;

  for (ix = sizeof(long) - 1; ix >= 0; ix--) {
    reg = (v >> (CHAR_BIT * ix)) & 0xFF;
    ch = (reg >> 4) & 0x0F;
    stream_putch(sp, s_hexval(ch, cap));
    ch = reg & 0x0F;
    stream_putch(sp, s_hexval(ch, cap));
  }
}

/*------------------------------------------------------------------------*/
static void s_putdbl(stream *sp, double dval, int prec) {
  byte fmt[8];
  byte tmp[64];
  int nw;

  if (prec == 0) prec = DBL_DIG;

  sprintf(fmt, "%%.%df", prec);
  nw = sprintf(tmp, fmt, dval);

  stream_write(sp, tmp, nw);
}

/*------------------------------------------------------------------------*/
static char s_hexval(int ch, int cap) {
  int abase = (cap ? 'A' : 'a');

  if (ch >= 0 && ch <= 9)
    return ch + '0';
  else if (ch >= 10 && ch <= 15)
    return ch - 10 + abase;
  else
    return '?';
}

/*------------------------------------------------------------------------*/
char *s_cvt_num(long val, int minwidth, char pad) {
  int lpad = 1;
  int start = 0, pos, save, ndig = 0;

  /* Figure out which side padding goes on  */
  if (minwidth < 0) {
    lpad = 0;
    minwidth = -minwidth;
  }

  /* Deal with negatives and zero           */
  if (val < 0) {
    s_numbuf[start++] = '-';
    val = -val;
    ++ndig;
  } else if (val == 0) {
    s_numbuf[start++] = '0';
    ++ndig;
  }

  /* Generate digits in reverse order       */
  pos = start;
  while (val) {
    s_numbuf[pos++] = (val % 10) + '0';
    val /= 10;
    ++ndig;
  }
  save = pos--; /* save for right-padding  */

  /* If we're left-padding, add pad chars   */
  if (lpad) {
    while (ndig < minwidth) {
      s_numbuf[++pos] = pad;
      ++ndig;
    }
    s_numbuf[pos + 1] = '\0';
  }

  /* Reverse digits (and pads, if needed)   */
  while (pos > start) {
    char tmp = s_numbuf[start];

    s_numbuf[start] = s_numbuf[pos];
    s_numbuf[pos] = tmp;

    ++start;
    --pos;
  }

  /* If we're right-padding, add pad chars  */
  if (!lpad) {
    while (ndig < minwidth) {
      s_numbuf[save++] = pad;
      ++ndig;
    }
    s_numbuf[save] = '\0';
  }

  return s_numbuf;

} /* end of s_cvt_num() */

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
