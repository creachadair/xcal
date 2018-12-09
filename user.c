/*
    user.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    User connection record structure and ancillary routines for
    XCaliber Mark II.
 */

#include "user.h"
#include "con.h"
#include "net.h"
#include "telnet.h"

#ifdef DEBUG
#include <stdio.h>
#endif
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#include "mem.h"
#endif

#define STREAM_LEN 32

/*------------------------------------------------------------------------*/
void user_init(uptr up, int sz) {
  up->sd = -1;
  up->port = 0;
  up->ip = 0;
  stream_init(&(up->in), STREAM_LEN);
  up->mqt = up->mqh = NULL;
  up->inc = NULL;
  stream_init(&(up->out), STREAM_LEN);
  up->name[0] = '\0';
  up->bits = 0;
  up->opts = 0;
  up->mstr = 0;
  up->iglen = (sz / CHAR_BIT) + ((sz % CHAR_BIT) ? 1 : 0);
  up->igmap = calloc(up->iglen, sizeof(byte));
  up->when = time(NULL);
}

/*------------------------------------------------------------------------*/
void user_clear(uptr up) {
  mptr mp;

  up->when = 0;
  free(up->igmap);
  up->igmap = NULL;
  up->iglen = 0;
  up->mstr = 0;
  up->bits = 0;
  up->name[0] = '\0';
  up->opts = 0;
  msg_clear(up->inc);
  stream_clear(&(up->out));
  up->inc = NULL;
  stream_clear(&(up->out));
  while ((mp = user_dequeue(up)) != NULL) msg_clear(mp);
  up->mqh = up->mqt = NULL;
  stream_clear(&(up->in));
  up->ip = 0;
  up->port = 0;
  up->sd = -1;
}

/*------------------------------------------------------------------------*/
int user_poll(uptr up, int rd) {
  char buf[80], *tmp;
  int nr;

#ifdef DEBUG2
  fprintf(stderr, "user_poll: 0x%p\n", up);
#endif
  if (rd) {
    if ((nr = net_read(up->sd, buf, 80)) <= 0) return 0;

#ifdef DEBUG
    fprintf(stderr, "user_poll: %d bytes read\n", nr);
#endif

    stream_write(&(up->in), buf, nr);
  }

RETRY:
  if (!user_handle_telnet(up)) return 0;
  if ((nr = stream_hasline(&(up->in))) != 0) {
    switch (STATE(up)) {
        /* Sitting idle...        */
      case IDLE_ST:
      case LINE_ST:
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in IDLE/LINE\n");
#endif
        SETSTATE(up, CMD_ST);
        break;

        /* Composing a message... */
      case BUILD_ST:
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in BUILD\n");
#endif
        tmp = stream_readln(&(up->in));
        if (tmp[0] == '\0') {
          int pos;

          free(tmp);
          tmp = stream_copy(MSTR(up->inc));
          pos = stream_length(MSTR(up->inc));
          pos -= 4;
          if (pos >= 0 && tmp[pos] == '%' && tolower(tmp[pos + 1]) == 'k') {
#ifdef DEBUG
            fprintf(stderr, "user_poll: abort string espied at pos=%d\n", pos);
#endif
            user_abort_msg(up);
            net_write(up->sd, XMSG(mnsmsg));
            SETSTATE(up, IDLE_ST);
          } else {
            SETSTATE(up, DONE_ST);
          }
        } else {
          msg_write(up->inc, tmp);
          msg_append(up->inc, '\r');
          msg_append(up->inc, '\n');
          free(tmp);
          goto RETRY;
        }
        free(tmp);
        break;

        /* Logging on...                */
      case LOGIN_ST:
        tmp = stream_readln(&(up->in));
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in LOGIN: %s\n", tmp);
#endif
        if (tolower(tmp[0]) == 'r' && tolower(tmp[1]) == 'p' &&
            tmp[2] == '\0' && !ISRP(up)) {
          SETRP(up);
          net_write(up->sd, XMSG(rpmsg));
          net_write(up->sd, XMSG(entname));
        } else {
          user_set_name(up, tmp);
          SETSTATE(up, LOGGED_ST);
        }
        free(tmp);
#ifdef DEBUG
        fprintf(stderr, "user_poll: end of LOGIN\n");
#endif
        break;

        /* Changing identity...         */
      case CHG_ST:
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in CHG\n");
#endif
        tmp = stream_readln(&(up->in));
        user_set_name(up, tmp);
        free(tmp);
        SETSTATE(up, CHGD_ST);
        break;

        /* Returning from being out     */
      case OUT_ST:
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in OUT\n");
#endif
        tmp = stream_readln(&(up->in));
        free(tmp);
        SETSTATE(up, IDLE_ST);
        break;

      case WARN_ST:
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in WARN\n");
#endif
        SETSTATE(up, WARND_ST);
        break;

        /* Some other state...          */
      default:
#ifdef DEBUG
        fprintf(stderr, "user_poll: got line in other state\n");
#endif
        break;
    } /* end switch(STATE(up)) */
  }

  return 1;
}

/*------------------------------------------------------------------------*/
void user_set_port(uptr up, short port) { up->port = port; }

void user_set_ip(uptr up, int ip) { up->ip = ip; }

/*------------------------------------------------------------------------*/
void user_set_name(uptr up, char *name) {
  int ix = 0;

  if (!name) {
#ifdef DEBUG
    fprintf(stderr, "user_set_name: name is null!\n");
#endif
    return;
  }
#ifdef DEBUG
  fprintf(stderr,
          "user_set_name: name is %s\n"
          "NAMELEN = %d ix = %d\n",
          name, NAMELEN, ix);
#endif

  while (name[ix] && ix < NAMELEN - 1) {
    up->name[ix] = name[ix];
    ++ix;
  }
  up->name[ix] = '\0';
}

/*------------------------------------------------------------------------*/
void user_ignore(uptr up, int who) {
  if (who == ALL) {
    int ix;

    for (ix = 0; ix < up->iglen; ix++) up->igmap[ix] = 0xFF;
  } else {
    up->igmap[who / CHAR_BIT] |= (1 << (who % CHAR_BIT));
  }
}

/*------------------------------------------------------------------------*/
void user_accept(uptr up, int who) {
  if (who == ALL) {
    int ix;

    for (ix = 0; ix < up->iglen; ix++) up->igmap[ix] = 0;
  } else {
    up->igmap[who / CHAR_BIT] &= ~(1 << (who % CHAR_BIT));
  }
}

/*------------------------------------------------------------------------*/
int user_igging(uptr up, int who) {
  return (up->igmap[who / CHAR_BIT] & (1 << (who % CHAR_BIT)));
}

/*------------------------------------------------------------------------*/
void user_start_msg(uptr up) {
  if (up->inc) user_abort_msg(up);

  up->inc = msg_alloc(MSG_LEN);
}

/*------------------------------------------------------------------------*/
void user_add_recipient(uptr up, int rcpt) { msg_rcpt(up->inc, rcpt); }

/*------------------------------------------------------------------------*/
void user_del_recipient(uptr up, int rcpt) { msg_unrcpt(up->inc, rcpt); }

/*------------------------------------------------------------------------*/
void user_abort_msg(uptr up) {
  if (up->inc) {
    msg_clear(up->inc);
    up->inc = NULL;
  }
}

/*------------------------------------------------------------------------*/
mptr user_end_msg(uptr up) {
  mptr mp = up->inc;

  up->inc = NULL;
  return mp;
}

/*------------------------------------------------------------------------*/
int user_handle_telnet(uptr up) {
  char tr, to;

  while (tel_hasreq(&(up->in), &tr, &to)) {
    switch (tr) {
      case WILL:
        tel_send(up->sd, DONT, to);
        break;
      case DO:
        tel_send(up->sd, WONT, to);
        break;
      case BRK:
      case INTR:
        net_write(up->sd, XMSG(brkmsg));
        if (!ISRB(up) || ISMASTER(up)) return 0;
        break;
      case POKE:
        net_write(up->sd, XMSG(aytmsg));
        break;
      default:
        break;
    }
  }
  return 1;
}

/*------------------------------------------------------------------------*/
void user_force(uptr up, mptr mp) {
  qptr qp = calloc(1, sizeof(qelt));

  qp->msg = msg_copy(mp);
  qp->link = up->mqh;
  up->mqh = qp;

  if (up->mqt == NULL) up->mqt = qp;
}

/*------------------------------------------------------------------------*/
void user_enqueue(uptr up, mptr mp) {
  qptr qp = calloc(1, sizeof(qelt));

  qp->msg = msg_copy(mp);
  qp->link = NULL;

  if (up->mqt != NULL) (up->mqt)->link = qp;
  up->mqt = qp;

  if (up->mqh == NULL) up->mqh = qp;
}

/*------------------------------------------------------------------------*/
mptr user_dequeue(uptr up) {
  qptr qp = up->mqh;

  if (qp) {
    mptr mp;

    up->mqh = qp->link;
    if (up->mqh == NULL) up->mqt = NULL;

    mp = qp->msg;
    free(qp);

    return mp;
  } else {
    return NULL;
  }
}
