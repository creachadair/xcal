/*
    stream.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    A bytestream data structure
 */

#ifndef XCAL_STREAM_H_
#define XCAL_STREAM_H_

typedef char   byte;

typedef struct {
  byte         *data;
  int  	        head;
  int  	        tail;
  int  	        num;
  int   	size;
} stream;

void	stream_init(stream *sp, int size);
void	stream_clear(stream *sp);
void	stream_grow(stream *sp, int size);
void    stream_shrink(stream *sp);

int 	stream_empty(stream *sp);
int 	stream_full(stream *sp);
int   	stream_length(stream *sp);
int     stream_hasline(stream *sp);

int 	stream_getch(stream *sp, byte *b);
int     stream_peekch(stream *sp, byte *b);
void	stream_putch(stream *sp, byte b);

int     stream_read(stream *sp, byte *b, int len);
byte   *stream_readln(stream *sp);
int     stream_getd(stream *sp, double *d);

void    stream_write(stream *sp, byte *b, int len);

void	stream_force(stream *sp, byte *b, int len);
void	stream_ungetch(stream *sp, byte b);
void	stream_flush(stream *sp);

void	stream_printf(stream *sp, char *fmt, ...);
byte   *stream_copy(stream *sp);
void	stream_extract(stream *sp, byte *buf);

#endif	/* end XCAL_STREAM_H_ */
