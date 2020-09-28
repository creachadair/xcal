/*
    con.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Conference structures for Xcaliber Mark II
 */

#include "con.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cmd.h"
#include "log.h"
#include "net.h"
#include "stream.h"

#ifdef DEBUG
#include "mem.h"
#endif

#define NEED_GETDTABLEHI
#ifdef NEED_GETDTABLEHI
/*
  If you are unfortunate enough not to have the very useful function
  getdtablehi() in your system's repertoire, you can define this macro
  to use the more commonly-defined getdtablesize()
 */
#define getdtablehi() getdtablesize()
#endif

extern char *g_prog;
extern int g_termwarn;
extern int g_quit;
static jmp_buf esc;

/*------------------------------------------------------------------------*/
/* Initialize a new conference structure                                  */

void con_init(con *cp, int size, short port, int trblk, int ear) {
  cp->who = calloc(size, sizeof(user *));
  cp->size = size;
  cp->left = calloc(2, sizeof(user *));
  cp->nml = 0;
  cp->mnl = 2;
  cp->num = cp->max = 0;
  if (ear) cp->ear = net_listen(port);
  cp->warn = NULL;
  cp->bits = 0;
  cp->trblk = trblk;
  SETNEWCON(cp);
  cp->up = 0;
  cp->til = 0;
  cp->new = 0;
}

/*------------------------------------------------------------------------*/
/* Clean up the conference structure                                      */

void con_reset(con *cp) {
  if (cp) {
    int ix;

    msg_clear(cp->warn);
    cp->warn = NULL;
    free(cp->who);
    cp->who = NULL;
    cp->size = 0;
    for (ix = 0; ix < cp->nml; ix++) user_clear(cp->left[ix]);
    free(cp->left);
    cp->nml = 0;
    cp->mnl = 0;
    cp->bits = 0;
    CLRNEWCON(cp);
    cp->up = 0;
    cp->til = 0;
    cp->new = 0;
  }
}

void con_clear(con *cp) {
  if (cp) {
    con_shutdown(cp, 1);
    con_reset(cp);
    cp->trblk = 0;
    cp->ear = -1;
  }
}

/*------------------------------------------------------------------------*/
/* Shut down the conference, disconnecting all users                      */

void con_shutdown(con *cp, int e2) {
  int ix;

  /* Close the listener too? */
  if (e2) net_close(cp->ear);

  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) != NULL && USER(cp, ix)->sd >= 0) {
      net_write(USER(cp, ix)->sd, XMSG(conterm));
      net_close(USER(cp, ix)->sd);
      user_clear(USER(cp, ix));
      USER(cp, ix) = NULL;
    }
  }

  for (ix = 0; ix < cp->nml; ix++) {
    if (cp->left[ix] != NULL) user_clear(cp->left[ix]);
  }

  log_startup(0);
}

/*------------------------------------------------------------------------*/
void con_left(con *cp, user *up) {
  cp->left[cp->nml++] = up;

  if (cp->nml >= cp->mnl) {
    int ix;
    user **tmp = calloc(2 * cp->mnl, sizeof(user *));

    for (ix = 0; ix < cp->nml; ix++) {
      tmp[ix] = cp->left[ix];
    }
    free(cp->left);
    cp->left = tmp;
    cp->mnl *= 2;
  }
}

/*------------------------------------------------------------------------*/
/* This is the main loop for the conference                               */
void con_run(con *cp, int min) {
  fd_set rds, wds;
  int ix, neq, nd, done = 0;
  time_t start, now, end, warn;
  struct timeval timeout;

  log_startup(1);
  start = cp->up = time(NULL);
  end = start + (min * 60);
  warn = end - g_termwarn;
  cp->til = end;

  while (!done && !g_quit) {
    FD_ZERO(&rds);
    FD_ZERO(&wds);
    FD_SET(cp->ear, &rds);

    /*
      If any connexions have messages in their queues, give them a shot
      at delivery (assuming the users in question aren't busy).  We count
      up the number of non-busy users that still have messages to deliver
     */
    neq = 0; /* number of connexions with non-empty message queues */
    for (ix = 0; ix < cp->size; ix++) {
      if (!ISALIVE(USER(cp, ix))) continue;

#ifdef DEBUG2
      fprintf(stderr, "con_run: user %d state is %d\n", ix,
              STATE(USER(cp, ix)));
#endif
      if (ISIDLE(USER(cp, ix))) {
        if (USER(cp, ix)->mqh != NULL) { /* does user have msgs? */
          user_transmit(cp, ix);         /* start one message    */
        }
      }

      /* If user still has messages to receive, count him       */
      if (USER(cp, ix)->mqh != NULL && ISIDLE(USER(cp, ix))) ++neq;

      if (RECEIVING(USER(cp, ix))) {
        FD_SET(USER(cp, ix)->sd, &wds);
      }

      FD_SET(USER(cp, ix)->sd, &rds); /* watch for user input  */
    }                                 /* end for() */

    /*
      At this point, we've given everyone a chance to have a message
      sent, if they're idle.  Now, we give everyone a chance to be
      polled for input and issue new commands (we have to alternate
      these steps to avoid deadlocking on a busy person)

      If nobody is waiting for a message to be delivered, we'll just
      wait indefinitely for input; otherwise, we'll just poll quickly
      and then go back to sending messages (but 'indefinitely' in this
      case means 'until the next warning')
     */
#ifdef DEBUG
    fprintf(stderr, "con_run: %d non-empty queues\n", neq);
#endif
    if (neq != 0) {
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      nd = select(getdtablehi(), &rds, &wds, NULL, &timeout);
    } else {
      timeout.tv_sec = labs(warn - time(NULL));
      timeout.tv_usec = 0;
      nd = select(getdtablehi(), &rds, &wds, NULL, &timeout);
    }

    /* Check for incoming connexions ... */
    if (FD_ISSET(cp->ear, &rds)) {
      con_newuser(cp);
    }

    /*
      Loop over the conference, figuring out who had input and letting
      them deal with it
     */
    for (ix = 0; ix < cp->size; ix++) {
      /* Skip those who don't need it */
      if (!ISALIVE(USER(cp, ix))) continue;

#ifdef DEBUG2
      fprintf(stderr, "con_run: polling user 0x%p in slot %d\n", USER(cp, ix),
              ix);
#endif
      if (nd > 0 && FD_ISSET(USER(cp, ix)->sd, &rds)) {
        if (!user_poll(USER(cp, ix), 1)) {
          if (ISMASTER(USER(cp, ix))) done = 1;
          con_exuser(cp, ix, DIED);
          continue;
        }
      } else {
        if (!user_poll(USER(cp, ix), 0)) {
          if (ISMASTER(USER(cp, ix))) done = 1;
          con_exuser(cp, ix, DIED);
          continue;
        }
      }

#ifdef DEBUG2
      fprintf(stderr, "con_run: after poll, user %d state is %d\n", ix,
              STATE(USER(cp, ix)));
#endif
      /* Now, do I as supervisor have to do anything? */
      switch (STATE(USER(cp, ix))) {
        case DONE_ST: /* done composing -- handle delivery   */
          con_deliver(cp, ix);
          break;

        case CMD_ST: /* has a command -- parse and do it    */
          con_command(cp, ix, &done);
          break;

        case LOGGED_ST: /* done entering name -- alert media   */
          con_login(cp, ix);
          break;

        case CHGD_ST: /* done changing name -- alert media   */
          con_change(cp, ix);
          break;

        case WARND_ST: /* done composing warning -- post it   */
          con_newwarn(cp, ix);
          break;

        case RECV_ST: /* receiving a message -- send some    */
        case EXPL_ST:
        case READI_ST:
          if (FD_ISSET(USER(cp, ix)->sd, &wds)) {
            if (setjmp(esc) == 0) user_sendblk(cp, ix);
          }
          break;

        default: /* anything else -- we don't much care */
          break;
      }
    } /* end for() */

    if (cp->til > end) {
      end = cp->til;
      warn = end - g_termwarn;
    }
    /* Check the time -- is it time to quit the con yet? */
    now = time(NULL);
    if (now > end)
      done = 1;
    else if (now > warn) { /* post warnings as appropriate */
      con_warnsd(cp, end - now);
      warn += 60;
    }
  } /* end while(!done) */

} /* end con_loop() */

/*------------------------------------------------------------------------*/
void con_warnsd(con *cp, int when) {
  int nm;
  stream st;
  char *tmp;

  stream_init(&st, 32);

  nm = (when / 60) + ((when % 60) ? 1 : 0);
  fprintf(stderr, "%s: %d minute warning issued\n", g_prog, nm);

  if (nm == 0)
    stream_printf(&st, MSG(timewrn), (when % 60), MSG(secplur));
  else
    stream_printf(&st, MSG(timewrn), nm,
                  ((nm == 1) ? MSG(minsing) : MSG(minplur)));
  tmp = stream_copy(&st);
  stream_clear(&st);

  con_warn(cp, tmp);
  free(tmp);
}

/*------------------------------------------------------------------------*/
void con_newwarn(con *cp, int who) {
  char *tmp;

  tmp = stream_readln(&(USER(cp, who)->in));
  con_warn(cp, tmp);
  free(tmp);
  SETSTATE(USER(cp, who), IDLE_ST);
}

/*------------------------------------------------------------------------*/
void con_deliver(con *cp, int who) {
  mptr mp;
  int res;
  stream st;
  char *tmp;

  mp = user_end_msg(USER(cp, who));
#ifdef DEBUG
  fprintf(stderr, "con_deliver: who=%d, mp=0x%p\n", who, mp);
#endif

  if (stream_length(MSTR(mp)) == 0) {
#ifdef DEBUG
    fprintf(stderr, "con_deliver: message length is zero, cancelling\n");
#endif
    net_write(USER(cp, who)->sd, XMSG(empty));
    msg_clear(mp);
    SETSTATE(USER(cp, who), IDLE_ST);
    return;
  }
  stream_init(&st, 16);
  if (msg_toall(mp)) {
    stream_printf(&st, MSG(allfrom), who, USER(cp, who)->name);
  } else {
    stream_printf(&st, MSG(msgfrom), who, USER(cp, who)->name);
  }
  tmp = stream_copy(&st);
  stream_force(MSTR(mp), tmp, 0);
  stream_clear(&st);

  res = con_post(cp, mp);
  if (res != 0) {
    net_write(USER(cp, who)->sd, XMSG(sentmsg));
  } else {
    net_write(USER(cp, who)->sd, XMSG(norcpt));
  }

  msg_clear(mp);
  USER(cp, who)->inc = NULL;
  SETSTATE(USER(cp, who), IDLE_ST);
}

/*------------------------------------------------------------------------*/
void con_login(con *cp, int who) {
  stream st;
  char *tmp;

  stream_init(&st, 256);
  stream_printf(&st, MSG(newuser), who, USER(cp, who)->name);
  tmp = stream_copy(&st);

#ifdef DEBUG
  fprintf(stderr, "%s", tmp);
#endif
  con_notify(cp, who, tmp);
  free(tmp);

  stream_flush(&st);
  stream_printf(&st, "%s", MSG(intro));
  stream_printf(&st, MSG(tlkwith), USER(cp, who)->mstr,
                USER(cp, USER(cp, who)->mstr)->name);
  tmp = stream_copy(&st);

  net_write(USER(cp, who)->sd, tmp, strlen(tmp));
  free(tmp);

  stream_clear(&st);
  CLRRN(USER(cp, who));
  SETSTATE(USER(cp, who), IDLE_ST);
}

/*------------------------------------------------------------------------*/
void con_change(con *cp, int who) {
  stream st;
  char *tmp;

  stream_init(&st, 16);
  stream_printf(&st, MSG(namenot), who, USER(cp, who)->name);
  tmp = stream_copy(&st);
  stream_clear(&st);

  con_notify(cp, who, tmp);
  free(tmp);

  net_write(USER(cp, who)->sd, XMSG(donemsg));

  SETSTATE(USER(cp, who), IDLE_ST);
}

/*------------------------------------------------------------------------*/
/* Routines to maintain the bounce list                                   */

int con_isbounced(con *cp, int ip, short port) {
  int ix;

  for (ix = 0; ix < BOUNCETAB; ix++) {
    if (cp->bo[ix].ip == ip && cp->bo[ix].port == port) return ix + 1;
  }

  return 0;
}

int con_bounce(con *cp, int ip, short port) {
  int ix;

  for (ix = 0; ix < BOUNCETAB; ix++) {
    if (cp->bo[ix].ip == 0) break;
  }
  if (ix >= BOUNCETAB) return -1;

  cp->bo[ix].ip = ip;
  cp->bo[ix].port = port;
  return ix;
}

void con_nbounce(con *cp, int ip, short port) {
  int ix = con_isbounced(cp, ip, port);

  if (ix--) {
    cp->bo[ix].ip = 0;
    cp->bo[ix].port = 0;
    cp->bo[ix].hide = 0;
  }
}

int con_numbounced(con *cp) {
  int ix, nb = 0;

  for (ix = 0; ix < BOUNCETAB; ix++) {
    if (cp->bo[ix].ip != 0) ++nb;
  }

  return nb;
}

/*------------------------------------------------------------------------*/
/* Accept a new user to the con, and set up h(is/er) record.  If the con
   is full, tell the poor sap so, and disconnect.                         */

void con_newuser(con *cp) {
  int np, nd, ip;
  short port;
  user *nup;

  for (np = 0; np < cp->size; np++) {
    if (USER(cp, np) == NULL) /* never been used before */
      break;
    else if (USER(cp, np)->sd < 0) { /* used by a dead user    */
      user_clear(USER(cp, np));
      break;
    }
  }

  if ((nd = net_accept(cp->ear, &ip, &port)) < 0) return;

  if (con_isbounced(cp, ip, port)) {
    fprintf(stderr, "%s: denied bounced address %d.%d.%d.%d port %d\n", g_prog,
            (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
            ntohs(port));
    net_write(nd, XMSG(bounced));
    close(nd);
    return;
  }
  if (np >= cp->size) {
    fprintf(stderr, "%s: conference full, denied %d.%d.%d.%d port %d\n", g_prog,
            (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
            ntohs(port));
    net_write(nd, XMSG(confull));
    close(nd);
    return;
  }

  nup = calloc(1, sizeof(user));
  user_init(nup, cp->size);
  user_set_port(nup, port);
  user_set_ip(nup, ip);
  nup->sd = nd;
  SETSTATE(nup, LOGIN_ST);
  SETRC(nup);
  SETRN(nup); /* initially reject notifications */
  SETTELLALL(nup);

  if (ISNEWCON(cp)) {
    fprintf(stderr, "%s: new master at #%d from %d.%d.%d.%d port %u\n", g_prog,
            np, (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF,
            ip & 0xFF, ntohs(port));
    SETMASTER(nup);
    SETGETNEW(nup);
    net_write(nd, XMSG(mwelc));
    CLRNEWCON(cp);
    cp->new = np;
    nup->mstr = np;
  } else {
    fprintf(stderr, "%s: new user under #%d at #%d from %d.%d.%d.%d port %u\n",
            g_prog, cp->new, np, (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF, ip & 0xFF, ntohs(port));
    nup->mstr = cp->new; /* master of new connexions */
    net_write(nd, XMSG(welcome));
  }
  log_connect(ip, port, np);

#ifdef DEBUG
  fprintf(stderr, "con_newuser: user bits: 0x%04X\n", nup->bits);
#endif
  USER(cp, np) = nup;

  /* If there's a warning posted, make sure they get it right away */
  if (cp->warn) user_enqueue(nup, cp->warn);

#ifdef DEBUG
  fprintf(stderr, "con_newuser: done creating user 0x%p\n", nup);
#endif
  /* Keep statistics, for the 'info' command */
  ++cp->num;
  if (cp->num > cp->max) cp->max = cp->num;
}

/*------------------------------------------------------------------------*/
/* Clean up after a user who has disconnected, either intentionally or
   through the vagaries of the network                                    */

void con_exuser(con *cp, int who, int how) {
  stream ns;
  char *tmp;
  mptr mp;

  if (USER(cp, who) == NULL) return;

  stream_init(&ns, 32);
  switch (how) {
    case KILLED:
      stream_printf(&ns, MSG(killnot), who, USER(cp, who)->name);
      SETKILLED(USER(cp, who));
      log_kill(who, USER(cp, who)->mstr);
      break;
    default:
      stream_printf(&ns, MSG(exnot), who, USER(cp, who)->name);
      log_disconnect(who);
  }
  tmp = stream_copy(&ns);
  stream_clear(&ns);

  /*
    If the user who (was) disconnected was a submaster, we have to
    transfer his or her subcon members back to their parent.
   */
  if (ISSUBMASTER(USER(cp, who))) con_normalize(cp, who);

#ifdef DEBUG
  fprintf(stderr, "con_exuser: %s", tmp);
#endif
  /*
    Users who died are not destroyed just yet -- we keep them around
    with a bad socket descriptor so that the 'left' command can tell
    who has left recently and in what circumstances
   */
  net_write(USER(cp, who)->sd, XMSG(discmsg));
  net_close(USER(cp, who)->sd);
  USER(cp, who)->sd = -1; /* invalidate their socket */
  CLRMASTER(USER(cp, who));
  CLRPRIV(USER(cp, who));
  CLRSUBMASTER(USER(cp, who));
  USER(cp, who)->when = time(NULL);     /* timestamp their leaving */
  user_abort_msg(USER(cp, who));        /* flush their composing   */
  stream_flush(&(USER(cp, who)->out));  /* flush their out stream  */
  stream_shrink(&(USER(cp, who)->out)); /* attenuate the stream    */

  /* Get rid of any pending undelivered messages in their queue    */
  while ((mp = user_dequeue(USER(cp, who))) != NULL) msg_clear(mp);

  SETSTATE(USER(cp, who), DEAD_ST);
  con_left(cp, USER(cp, who)); /* mark them into the left list    */
  con_notify(cp, who, tmp);    /* post departure notification     */

  if (how == KILLED) {
    fprintf(stderr, "%s: user killed at #%d by #%d\n", g_prog, who,
            USER(cp, who)->mstr);
  } else {
    fprintf(stderr, "%s: user disconnected at #%d\n", g_prog, who);
  }
  USER(cp, who) = NULL; /* release them from the con       */

  --cp->num; /* we now have one less user on the conference :(    */
  free(tmp);
}

/*------------------------------------------------------------------------*/
/* This is called to transfer "ownership" of a bitmap of users, including
   all the appropriate messages and such                                  */

void con_give_map(con *cp, int who, byte *map) {
  int ix, ne = 0;
  mptr mp, nf;

  nf = msg_alloc(strlen(MSG(tlkwith)));
  msg_type(nf, M_NOTIFY);
  msg_write(nf, "\r\n");
  stream_printf(MSTR(nf), MSG(tlkwith), who, USER(cp, who)->name);

  mp = msg_alloc(strlen(MSG(passed)));
  msg_type(nf, M_NOTIFY);
  msg_write(mp, MSG(passed));

  /*
    Set each user's master to be 'who', and enqueue a message to that
    user saying 'You are talking with...'.  Then also add the user's
    who entry to the 'You have been passed' message that goes to the
    passee at the end
   */
  if (MAPBIT(map, 0)) {
    cp->new = USER(cp, who)->mstr;
    stream_write(MSTR(mp), XMSG(pnumsg));
    ++ne;
  }
  for (ix = 1; ix < cp->size; ix++) {
    if (MAPBIT(map, ix)) {
      ++ne;

      USER(cp, ix)->mstr = who;
      user_enqueue(USER(cp, ix), nf); /* ...you are talking with... */

      /* You have been passed ... */
      cmd_who_entry(cp, ix, who, MSTR(mp), FMT_STAT | FMT_NAME);
    }
  }

  if (ne != 0) user_enqueue(USER(cp, who), mp);

  msg_clear(mp);
  msg_clear(nf);
}

/*------------------------------------------------------------------------*/
/* This is called to transfer ownership of a normalized or disconnected
   submaster's subconference users back to his or her superior            */

void con_normalize(con *cp, int who) {
  int ix, ne = 0;
  byte *map;

  map = calloc(MAPSIZE(cp), sizeof(byte));
  if (cp->new == who) {
    SETMAP(map, 0);
    ++ne;
  }

  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) != NULL) {
      if (USER(cp, ix)->mstr == who) {
#ifdef DEBUG
        fprintf(stderr, "con_normalize: master of #%d is #%d\n", ix, who);
#endif
        SETMAP(map, ix);
        ++ne;
      }
    }
  }
  fprintf(stderr, "%s: normalizing #%d (%d users to transfer)\n", g_prog, who,
          ne);

  if (ne != 0) {
    con_give_map(cp, USER(cp, who)->mstr, map);
  }
  free(map);
}

/*------------------------------------------------------------------------*/
/* Enqueue a notification for everyone except 'ex' who is not RN          */

void con_notify(con *cp, int ex, char *msg) {
  int ix;
  mptr mp;

#ifdef DEBUG
  fprintf(stderr, "con_notify: ex=%d: %s\n", ex, msg);
#endif

  mp = msg_alloc(strlen(msg));
  msg_type(mp, M_NOTIFY);
  msg_write(mp, msg);

  /*
    The 'except' user is taken to be the "reason" for the notification,
    and so the notification is posted ONLY to members of the same sub-
    con as that user.  For global warnings, use con_warn().
   */
  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) != NULL) {
      if ((ix == USER(cp, ex)->mstr ||
           USER(cp, ix)->mstr == USER(cp, ex)->mstr ||
           USER(cp, ix)->mstr == ex) &&
          ix != ex && !ISRN(USER(cp, ix)) && !ISOUT(USER(cp, ix))) {
        user_enqueue(USER(cp, ix), mp);
      }
    }
  }

  msg_clear(mp);
}

/*------------------------------------------------------------------------*/
/* Enqueue a message for everyone -- system warnings cannot be ignored    */

void con_warn(con *cp, char *msg) {
  int ix;
  mptr mp;

#ifdef DEBUG
  fprintf(stderr, "con_warn: %s\n", msg);
#endif

  mp = msg_alloc(strlen(msg));
  msg_type(mp, M_WARN);
  msg_write(mp, "\r\n");
  msg_write(mp, msg);
  msg_write(mp, "\r\n");

  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) != NULL && USER(cp, ix)->sd >= 0)
      user_enqueue(USER(cp, ix), mp);
  }

  msg_clear(cp->warn);
  cp->warn = mp;
}

/*------------------------------------------------------------------------*/
/* Post to all members of the con, return number of recipients            */

int con_post(con *cp, mptr msg) {
  int nr = 0, ix;

#ifdef DEBUG
  fprintf(stderr, "con_post: post msg 0x%p from %d\n", msg, msg->from);
#endif
  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) == NULL || USER(cp, ix)->sd < 0) continue;

    if (con_post_one(cp, msg, ix) == POST_OK) ++nr;
  }

  return nr;
}

/*------------------------------------------------------------------------*/
int con_post_one(con *cp, mptr msg, int who) {
  if (!msg_isrcpt(msg, who))
    return POST_NR;
  else if (USER(cp, who) == NULL || USER(cp, who)->sd < 0)
    return POST_DIS;
  else if (user_igging(USER(cp, who), msg->from))
    return POST_IG;
  else if (ISOUT(USER(cp, who)))
    return POST_OUT;

  user_enqueue(USER(cp, who), msg);
  return POST_OK;
}

/*------------------------------------------------------------------------*/
void user_transmit(con *cp, int who) {
  mptr mp;
  char *tmp;

  if (USER(cp, who) == NULL) {
#ifdef DEBUG
    fprintf(stderr, "user_transmit: null user at #%d, aborting transmit\n",
            who);
#endif
    return;
  }

  mp = user_dequeue(USER(cp, who));
  if (mp == NULL) return;

#ifdef DEBUG
  fprintf(stderr, "user_transmit: sending message 0x%p to #%d\n", mp, who);
#endif
  switch (mp->type) {
    case M_HELP:
      SETSTATE(USER(cp, who), READI_ST);
      break;
    case M_EXPL:
      SETSTATE(USER(cp, who), EXPL_ST);
      break;
    default:
      SETSTATE(USER(cp, who), RECV_ST);
      break;
  }

  stream_flush(&(USER(cp, who)->out));
  tmp = stream_copy(MSTR(mp));
  stream_write(&(USER(cp, who)->out), tmp, stream_length(MSTR(mp)));
  free(tmp);

  msg_clear(mp);
}

/*------------------------------------------------------------------------*/
void user_sendblk(con *cp, int who) {
  int ns;
  char *tmp;

  if ((ns = stream_length(&(USER(cp, who)->out))) == 0) {
#ifdef DEBUG
    fprintf(stderr, "user_sendblk: done transmitting, idling\n");
#endif
    SETSTATE(USER(cp, who), IDLE_ST);
    return;
  }

  /* Read the rest, or the block size, which ever is smaller */
  if (ns > cp->trblk) ns = cp->trblk;

#ifdef DEBUG
  fprintf(stderr, "user_sendblk: transmitting %d bytes\n", ns);
#endif
  tmp = calloc(ns, sizeof(char));
  stream_read(&(USER(cp, who)->out), tmp, ns);
  user_string(cp, who, tmp, ns);

  free(tmp);
}

/*------------------------------------------------------------------------*/
/* This is where we need to build in control stripping and other such
   niceties of output munging.  Right now, we just send the data as 'tis. */

void user_string(con *cp, int who, char *msg, int len) {
  /*
    If user is rejecting controls, we will apply a very primitive form
    of control stripping -- a simple two-finger linear scan over the
    input, converting it in-place to the same array sans control chars
   */
  if (ISRC(USER(cp, who))) {
    int ix, pos = 0;

    for (ix = 0; ix < len; ix++) {
      if (iscntrl(msg[ix])) {
        if (isspace(msg[ix])) msg[pos++] = msg[ix];
      } else {
        msg[pos++] = msg[ix];
      }
    }

    len = pos;
  }

  if (len == 0) return;

  if (net_write(USER(cp, who)->sd, msg, len) <= 0) {
    free(msg);
    longjmp(esc, errno);
  }
}
