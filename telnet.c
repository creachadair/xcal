/*
    telnet.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Routines for handling telnet commands in the input stream
 */

#include "telnet.h"

int tel_hasreq(stream *sp, char *req, char *opt) {
  char ch = 0;

  if (stream_length(sp) < 2) return 0;

  stream_getch(sp, &ch);
  if (ch != IAC) {
    stream_ungetch(sp, ch);
    return 0;
  }

  stream_getch(sp, req);
  switch (*req) {
    case DO:
    case DONT:
    case WILL:
    case WONT:
    case SB:
      if (!stream_getch(sp, opt)) *opt = 0;
      break;
    default:
      *opt = 0;
      break;
  }

  return 1;
}

void tel_send(int sd, char req, char opt) {
  char rbuf[3];
  int ns;

  rbuf[0] = IAC;
  rbuf[1] = req;

  switch (req) {
    case DO:
    case DONT:
    case WILL:
    case WONT:
      rbuf[2] = opt;
      ns = 3;
      break;
    default:
      ns = 2;
      break;
  }

  net_write(sd, rbuf, ns);
}
