/*
    message.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Message structure and manipulators for XCaliber Mark II
 */

#ifndef XCAL_MESSAGE_H_
#define XCAL_MESSAGE_H_

#include "stream.h"

#define MSG_LEN    32
#define RCPT_LEN   5
#define ALL        -1

#define M_MSG      0
#define M_TOALL    1
#define M_WARN     2
#define M_NOTIFY   3
#define M_HELP     4
#define M_EXPL     5

#define MSTR(M)   (&((M)->data))

typedef struct msg {
  int       from;
  stream    data;
  byte      rcpt[RCPT_LEN];
  byte      type;
  int       ref;
} message;

typedef message *mptr;

mptr    msg_alloc(int size);
void    msg_init(mptr mp, int size);
void    msg_clear(mptr mp);
mptr    msg_copy(mptr mp);

void    msg_from(mptr mp, int from);
void    msg_type(mptr mp, byte mt);
void    msg_rcpt(mptr mp, int to);
void    msg_unrcpt(mptr mp, int to);
int     msg_toall(mptr mp);
int     msg_isrcpt(mptr mp, int who);
int     msg_numrcpt(mptr mp);
void    msg_append(mptr mp, char ch);
void    msg_write(mptr mp, char *line);

#endif /* end XCAL_MESSAGE_H_ */
