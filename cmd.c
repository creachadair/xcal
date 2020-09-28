/*
    cmd.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Command parser and handlers for Xcaliber Mark II
 */

#include "cmd.h"

#include "con.h"
#include "net.h"
#include "port.h"
#include "stream.h"
#include "user.h"
#include "val.h"
#include "xhelp.h"

#ifdef DEBUG
#include <stdio.h>
#endif
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#ifdef DEBUG
#include "mem.h"
#endif

#include "default.h"

/* Tab expansion string for 'explain' command                          */
#define TABSTR DEFAULT_TABSTR

typedef void (*xcmd)(con *, int, int *, char *);

/*
  This is the command vector, mapping command names to the functions
  that execute them.  An asterisk in a command name means that if a
  string matches the command name up to that point, it matches the
  command.  So, for example, 'c*l*ock' can be invoked by typing 'c',
  'cl', or 'clock'.
 */
struct ctab {
  char *name;
  xcmd entry;
} cmdtab[] = {
    {"*t*e*ll", cmd_tell},    {"w*ho", cmd_who},      {"by*e", cmd_bye},
    {"p*ort", cmd_port},      {"id", cmd_id},         {"im*p", cmd_im},
    {"ra*ll", cmd_ra},        {"aa*ll", cmd_aa},      {"ig*nore", cmd_ig},
    {"ac*cept", cmd_ac},      {"an", cmd_an},         {"rn", cmd_rn},
    {"rp", cmd_rp},           {"rc", cmd_rc},         {"oc", cmd_oc},
    {"ou*t", cmd_out},        {"ti*me", cmd_time},    {"tt*y", cmd_tty},
    {"le*ft", cmd_left},      {"rb", cmd_rb},         {"ab", cmd_ab},
    {"c*l*ock", cmd_clock},   {"in*f*o", cmd_info},   {"dw", cmd_dw},
    {"us*ers", cmd_usrs},     {"nu*mber", cmd_usrs},  {"a", cmd_all},
    {"ev*erything", cmd_all}, {"wa*rn", cmd_warn},    {"bo", cmd_nimp},
    {"nb*o", cmd_nimp},       {"lb*o", cmd_nimp},     {"bu", cmd_nimp},
    {"ki*ll", cmd_kill},      {"al*l", cmd_all},      {"he*lp", cmd_help},
    {"en*able", cmd_en},      {"di*s*able", cmd_dis}, {"xw*h*o", cmd_xwho},
    {"l*in*e", cmd_lin},      {"mt*y", cmd_mty},      {"gi*ve", cmd_give},
    {"e*x*p*l*ain", cmd_exp}, {"no*r*m", cmd_norm},   {"st*op", cmd_bye},
    {"ve*r*s*ion", cmd_vers}, {"be*l*ow", cmd_below}, {"xdl", cmd_xdl},
    {"xlang", cmd_xlang},     {"xyzzy", cmd_xyzzy},   {"xtend", cmd_xtend},
    {"xreset", cmd_xreset},   {"ru*n*time", cmd_ru},  {"", 0}};

extern char *g_prog;  /* global program name   */
extern char *g_vers;  /* global version string */
extern char g_lang[]; /* current language      */

static int cmd_match(char *str);
static void cmd_who_map(con *cp, int who, byte *map, stream *sp, int format);
static int cmd_argmap(con *cp, int who, byte *map, char *cmd);
static int cmd_isok(char *cmd);

/*
  This is the main dispatcher for all commands issued by a particular
  user.  The 'who' parameter tells us which user issued the command
  (it is an index into the array of users in the con structure).  The
  flag parameter is used as an output parameter; if *flag is nonzero
  when this function returns, the conference will be terminated.  The
  flag pointer is passed to the command function when it is actually
  called to handle the command in question.

  The command dispatch is handled by looking up each command in the
  command vector (the array declared above).  If it is found, the
  resulting function pointer is used to dispatch the command.
 */
void con_command(con *cp, int who, int *flag) {
  char *cmd, save;
  int start, ix = 0, cx;

  cmd = stream_readln(&(USER(cp, who)->in));

#ifdef DEBUG
  fprintf(stderr, "con_command: raw string: %s\n", cmd);
#endif
  while (isspace(cmd[ix])) /* skip leading whitespace   */
    ++ix;

  start = ix;
  while (isalpha(cmd[ix])) /* find leading command name */
    ++ix;

  save = cmd[ix];
  cmd[ix] = '\0';
#ifdef DEBUG2
  fprintf(stderr, "con_command: prefix: %s\n", cmd + start);
#endif
  if (start == ix && save == '\0') {
    cx = cmd_match("im");
  } else if ((cx = cmd_match(cmd + start)) < 0) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    SETSTATE(USER(cp, who), IDLE_ST);
    free(cmd);
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "con_command: match: %s at 0x%p\n", cmdtab[cx].name,
          cmdtab[cx].entry);
#endif
  cmd[ix] = save;
  (cmdtab[cx].entry)(cp, who, flag, cmd + ix);

  /* If the state has not been re-set by the command handler,
     mark the user as idle (this is the default action) */
  if (USER(cp, who) != NULL && STATE(USER(cp, who)) == CMD_ST)
    SETSTATE(USER(cp, who), IDLE_ST);

  free(cmd);
}

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
/* HERE BEGINNETH THE COMMAND HANDLERS                                    *
 * Look on my works, ye Mighty, and despair!                              */
/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

/* --- Traditional Xcaliber commands */

void cmd_tell(con *cp, int who, int *flag, char *rest) {
  int ix, ne, type = M_MSG;
  byte *map;

  map = calloc(MAPSIZE(cp), sizeof(byte));

  /* Read command arguments into a bitmap */
  ne = cmd_argmap(cp, who, map, rest);

  /* If there was an error, just return                    */
  if (ne < 0) {
    free(map);
    return;
  }

  /* It's an invalid argument if the user selected himself */
  if (MAPBIT(map, who)) {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    free(map);
    return;
  }

  if (ne == 0) { /* A tell-all was issued */
    if (TELLALL(USER(cp, USER(cp, who)->mstr))) {
      for (ix = 0; ix < cp->size; ix++) {
        /*
          Figure out who your tell-all goes to -- it should go to everyone
          who is not who is neither ignoring you nor out, but who is in the
          same subconference (has the same master as you, or for whom you
          are the master)
         */

        if (USER(cp, ix) != NULL && !user_igging(USER(cp, ix), who) &&
            !ISOUT(USER(cp, ix)) && ix != who && !ISRA(USER(cp, ix)) &&
            ((USER(cp, ix)->mstr == USER(cp, who)->mstr) ||
             (USER(cp, ix)->mstr == who) || (ix == USER(cp, who)->mstr))) {
          SETMAP(map, ix);
          ++ne;
        }
      } /* end for */

      /* If that doesn't leave anybody, complain and exit */
      if (ne == 0) {
        net_write(USER(cp, who)->sd, XMSG(invarg));
        free(map);
        return;
      }
    } else { /* ... if tell-alls are NOT enabled */
      net_write(USER(cp, who)->sd, XMSG(noalls));
      free(map);
      return;
    }
    /* If we get here, all the users in the bitmap are
       valid recipients of this message */

    type = M_TOALL;
  } else { /* ... if it's NOT a tell-all       */
    for (ix = 0; ix < cp->size; ix++) {
      if (MAPBIT(map, ix)) {
        if (ix != USER(cp, who)->mstr &&
            USER(cp, ix)->mstr != USER(cp, who)->mstr &&
            USER(cp, ix)->mstr != who) {
          net_write(USER(cp, who)->sd, XMSG(invarg));
          free(map);
          return;
        } else if (user_igging(USER(cp, ix), who)) {
          net_write(USER(cp, who)->sd, XMSG(igmsg));
          free(map);
          return;
        } else if (ISOUT(USER(cp, ix))) {
          net_write(USER(cp, who)->sd, XMSG(outmsg));
          free(map);
          return;
        }
      }
    }
    /* If we get here, all the users in the bitmap are
       valid recipients of this message */
  }

  /* Now, mark the message for delivery to all recipients */
  user_start_msg(USER(cp, who));
  msg_type(USER(cp, who)->inc, type);
  msg_from(USER(cp, who)->inc, who);
  for (ix = 0; ix < cp->size; ix++) {
    if (MAPBIT(map, ix)) user_add_recipient(USER(cp, who), ix);
  }

  /* Is the message body given here on the command line?  */
  ix = 0;
  while (rest[ix] && rest[ix] != ';') /* scan for semi-colon */
    ++ix;

  if (rest[ix] == ';' && rest[ix + 1] != '\0') {
    msg_write(USER(cp, who)->inc, rest + ix + 1);
    msg_write(USER(cp, who)->inc, "\r\n");

    net_write(USER(cp, who)->sd, "\r\n", 2); /* extra CRLF */
    con_deliver(cp, who);
  } else { /* ... message is not on command line */
    net_write(USER(cp, who)->sd, XMSG(speak));
    SETSTATE(USER(cp, who), BUILD_ST);
  }

  free(map);
}

void cmd_who_raw(con *cp, int who, int *flag, char *rest) {
  byte *map;
  int ne, ix, tf;
  stream st;
  char *tmp;

  map = calloc(MAPSIZE(cp), sizeof(byte));

  ne = cmd_argmap(cp, who, map, rest);

  if (ne < 0) {
    free(map);
    return;
  }

#ifdef DEBUG
  fprintf(stderr, "cmd_who_raw: who=%d master(%d)=%d\n", who, who,
          USER(cp, who)->mstr);
#endif
  /*
    If no users are specified, you get a list of all the users;
    which sense of "all" is meant depends on the value of the
    *flag parameter...
   */
  if (ne == 0) {
    for (ix = 0; ix < cp->size; ix++) {
      if (USER(cp, ix) != NULL) {
        if (*flag & FMT_ALL) {
          tf = 1;
        } else if (*flag & FMT_BELOW) {
          if (*flag & FMT_SAME) {
            tf = (ix == USER(cp, who)->mstr || USER(cp, ix)->mstr == who ||
                  USER(cp, ix)->mstr == USER(cp, who)->mstr);
          } else {
            tf = (USER(cp, ix)->mstr == who || ix == who);
          }
        } else if (*flag & FMT_SAME) {
          if (*flag & FMT_BELOW) {
            tf = (ix == USER(cp, who)->mstr || USER(cp, ix)->mstr == who ||
                  USER(cp, ix)->mstr == USER(cp, who)->mstr);
          } else {
            tf = (USER(cp, ix)->mstr == USER(cp, who)->mstr);
          }
        } else {
          tf = 0;
        }
#ifdef DEBUG
        fprintf(stderr, "cmd_who_raw: ix=%d mix=%d fmt=%08X tf=%d\n", ix,
                USER(cp, ix)->mstr, *flag, tf);
#endif

        if (tf) {
          SETMAP(map, ix);
        }
      }
    }
    SETMAP(map, who);

  } else { /* ...otherwise, make sure the users specified are visible */
    for (ix = 0; ix < cp->size; ix++) {
      if (MAPBIT(map, ix)) {
        if (*flag & FMT_ALL) {
          tf = 1;
        } else if (*flag & FMT_BELOW) {
          if (*flag & FMT_SAME) {
            tf = (ix == USER(cp, who)->mstr || USER(cp, ix)->mstr == who ||
                  USER(cp, ix)->mstr == USER(cp, who)->mstr);
          } else {
            tf = (USER(cp, ix)->mstr == who || ix == who);
          }
        } else if (*flag & FMT_SAME) {
          if (*flag & FMT_BELOW) {
            tf = (ix == USER(cp, who)->mstr || USER(cp, ix)->mstr == who ||
                  USER(cp, ix)->mstr == USER(cp, who)->mstr);
          } else {
            tf = (USER(cp, ix)->mstr == USER(cp, who)->mstr);
          }
        } else {
          tf = 0;
        }
#ifdef DEBUG
        fprintf(stderr, "cmd_who_raw: fmt=%08X tf=%08X\n", *flag, tf);
#endif

        if (!tf) {
          net_write(USER(cp, who)->sd, XMSG(invarg));
          free(map);
          return;
        }
      }
    }
  }

  /*
    At this point, the bitmap should contain only users you can see the
    information for.  Send that off to con_who_map() to be generated
    into an output message
   */
  stream_init(&st, 32);
  stream_write(&st, "\r\n", 2);
  cmd_who_map(cp, who, map, &st, *flag);

  tmp = stream_copy(&st);
  net_write(USER(cp, who)->sd, tmp, stream_length(&st));

  stream_clear(&st);
  free(tmp);
  free(map);
}

void cmd_who(con *cp, int who, int *flag, char *rest) {
  int fmt = FMT_STAT | FMT_NAME | FMT_SUBCON;

  cmd_who_raw(cp, who, &fmt, rest);
}

void cmd_bye(con *cp, int who, int *flag, char *rest) {
  if (ISMASTER(USER(cp, who))) {
    *flag = 1;
  } else {
    con_exuser(cp, who, DIED);
  }
}

void cmd_port(con *cp, int who, int *flag, char *rest) {
  int fmt = FMT_PORT | FMT_NAME | FMT_SUBCON;

  if (ISRP(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(noport));
    return;
  }

  cmd_who_raw(cp, who, &fmt, rest);
}

void cmd_help(con *cp, int who, int *flag, char *rest) {
  cmd_exp(cp, who, flag, MSG(helptop));
}

void cmd_exp(con *cp, int who, int *flag, char *rest) {
  FILE *fp;
  int ix = 0, nr;
  mptr mp;
  char buf[64];

  while (isspace(rest[ix])) ++ix;

  switch (help_lookup(TOCFILE, rest + ix, &fp)) {
    case HELP_OK:
      mp = msg_alloc(64);

      msg_write(mp, "\r\n");
      while ((nr = fread(buf, sizeof(char), 64, fp)) > 0) {
        for (ix = 0; ix < nr; ix++) {
          if (buf[ix] == '\n')
            msg_append(mp, '\r');
          else if (buf[ix] == '\t') {
            msg_write(mp, TABSTR);
            continue;
          }
          msg_append(mp, buf[ix]);
        }
      }
      fclose(fp);
      msg_write(mp, "\r\n");
      user_force(USER(cp, who), mp);
      break;

    case HELP_NF:
      net_write(USER(cp, who)->sd, XMSG(cantexp));
      break;

    default:
      net_write(USER(cp, who)->sd, XMSG(fnfmsg));
      break;
  }
  SETSTATE(USER(cp, who), IDLE_ST);
}

void cmd_info(con *cp, int who, int *flag, char *rest) {
  stream st;
  char *tmp;
  struct rusage res;

  cmd_time(cp, who, flag, rest);

  stream_init(&st, 32);
  stream_printf(&st, MSG(infomsg), cp->num, cp->max, g_prog, g_vers);
  tmp = stream_copy(&st);
  net_write(USER(cp, who)->sd, tmp, stream_length(&st));
  free(tmp);

  getrusage(RUSAGE_SELF, &res);

  stream_flush(&st);
  stream_printf(&st, MSG(coresize), (unsigned)res.ru_maxrss);
  tmp = stream_copy(&st);
  net_write(USER(cp, who)->sd, tmp, stream_length(&st));
  free(tmp);

  stream_clear(&st);

  cmd_ru(cp, who, flag, rest);
}

void cmd_vers(con *cp, int who, int *flag, char *rest) {
  stream st;
  char *tmp;

  stream_init(&st, 32);
  stream_printf(&st, MSG(xcalver), g_vers);
  tmp = stream_copy(&st);

  net_write(USER(cp, who)->sd, tmp, stream_length(&st));
  stream_clear(&st);

  free(tmp);
}

void cmd_id(con *cp, int who, int *flag, char *rest) {
  int ix = 0;

  while (isspace(rest[ix])) ++ix;

  if (rest[ix] == '\0') {
    net_write(USER(cp, who)->sd, XMSG(newname));
    SETSTATE(USER(cp, who), CHG_ST);
  } else {
    user_set_name(USER(cp, who), rest + ix);
    con_change(cp, who);
  }
}

void cmd_im(con *cp, int who, int *flag, char *rest) {
  stream st;
  char *tmp;
  int ix;

  stream_init(&st, 32);
  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) != NULL && ISBUILDING(USER(cp, ix))) {
      if (msg_toall(USER(cp, ix)->inc)) {
        /* You see messages to all if you're not RA and you're a recipient,
           and you're ignoring the person who's building them.  You have to
           be a recipient because of subcons, which change the rules for
           sending to "all" */

        if (!ISRA(USER(cp, who)) && msg_isrcpt(USER(cp, ix)->inc, who) &&
            !user_igging(USER(cp, who), ix)) {
          cmd_who_entry(cp, ix, who, &st, FMT_STAT | FMT_NAME);
        }
      } else {
        /* You see messages to some if you're a recipient and you
           are not ignoring the person building them */

        if (msg_isrcpt(USER(cp, ix)->inc, who) &&
            !user_igging(USER(cp, who), ix)) {
          cmd_who_entry(cp, ix, who, &st, FMT_STAT | FMT_NAME);
        }
      }
    }
  }
  if (stream_length(&st) == 0) {
    net_write(USER(cp, who)->sd, XMSG(nomsgs));
  } else {
    tmp = stream_copy(&st);

    net_write(USER(cp, who)->sd, tmp, stream_length(&st));
    free(tmp);
  }
  stream_clear(&st);
}

/* --- Various state bits */

void cmd_ra(con *cp, int who, int *flag, char *rest) {
  SETRA(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_aa(con *cp, int who, int *flag, char *rest) {
  CLRRA(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_ig(con *cp, int who, int *flag, char *rest) {
  byte *map;
  int ix, ne;

  map = calloc(MAPSIZE(cp), sizeof(byte));

  ne = cmd_argmap(cp, who, map, rest);
  if (ne < 0) {
    free(map);
    return;
  }

  if (MAPBIT(map, who)) {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    free(map);
    return;
  }

  if (ne == 0) {
    user_ignore(USER(cp, who), ALL);
    user_accept(USER(cp, who), who);
  } else {
    for (ix = 0; ix < cp->size; ix++) {
      if (MAPBIT(map, ix)) {
        if (USER(cp, ix)->mstr == USER(cp, who)->mstr ||
            USER(cp, ix)->mstr == who) {
          user_ignore(USER(cp, who), ix);
        } else {
          net_write(USER(cp, who)->sd, XMSG(invarg));
          free(map);
          return;
        }
      }
    }
  }

  net_write(USER(cp, who)->sd, XMSG(donemsg));

  free(map);
}

void cmd_ac(con *cp, int who, int *flag, char *rest) {
  byte *map;
  int ix, ne;

  map = calloc(MAPSIZE(cp), sizeof(byte));

  ne = cmd_argmap(cp, who, map, rest);
  if (ne < 0) {
    free(map);
    return;
  }

  if (MAPBIT(map, who)) {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    free(map);
    return;
  }

  if (ne == 0) {
    user_accept(USER(cp, who), ALL);
  } else {
    for (ix = 0; ix < cp->size; ix++) {
      if (MAPBIT(map, ix)) {
        if (USER(cp, ix)->mstr == USER(cp, who)->mstr ||
            USER(cp, ix)->mstr == who) {
          user_accept(USER(cp, who), ix);
        } else {
          net_write(USER(cp, who)->sd, XMSG(invarg));
          free(map);
          return;
        }
      }
    }
  }

  net_write(USER(cp, who)->sd, XMSG(donemsg));

  free(map);
}

void cmd_an(con *cp, int who, int *flag, char *rest) {
  CLRRN(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_rn(con *cp, int who, int *flag, char *rest) {
  SETRN(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_rp(con *cp, int who, int *flag, char *rest) {
  SETRP(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(rpmsg));
}

void cmd_rc(con *cp, int who, int *flag, char *rest) {
  SETRC(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_oc(con *cp, int who, int *flag, char *rest) {
  CLRRC(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_rb(con *cp, int who, int *flag, char *rest) {
  SETRB(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_ab(con *cp, int who, int *flag, char *rest) {
  CLRRB(USER(cp, who));
  net_write(USER(cp, who)->sd, XMSG(donemsg));
}

void cmd_out(con *cp, int who, int *flag, char *rest) {
  mptr mp;

  while ((mp = user_dequeue(USER(cp, who))) != NULL) msg_clear(mp);

  net_write(USER(cp, who)->sd, XMSG(youout));
  SETSTATE(USER(cp, who), OUT_ST);
}

void cmd_lin(con *cp, int who, int *flag, char *rest) {
  net_write(USER(cp, who)->sd, XMSG(linmsg));
  SETSTATE(USER(cp, who), LINE_ST);
}

void cmd_time(con *cp, int who, int *flag, char *rest) {
  time_t when = cp->up;
  struct tm *dtr;
  stream st;
  char *tmp;

  dtr = localtime(&when);
  stream_init(&st, 32);
  stream_printf(&st, MSG(upat2),
                ((dtr->tm_hour <= 12) ? dtr->tm_hour : dtr->tm_hour - 12),
                dtr->tm_min, dtr->tm_sec,
                ((dtr->tm_hour < 12) ? MSG(antemer) : MSG(postmer)),
                dtr->tm_mon + 1, dtr->tm_mday,
                ((dtr->tm_year > 99) ? dtr->tm_year - 100 : dtr->tm_year));

  when = time(NULL);
  dtr = localtime(&when);

  stream_printf(&st, MSG(clkmsg),
                ((dtr->tm_hour <= 12) ? dtr->tm_hour : dtr->tm_hour - 12),
                dtr->tm_min, dtr->tm_sec,
                ((dtr->tm_hour < 12) ? MSG(antemer) : MSG(postmer)));

  when = cp->til - when;
  stream_printf(&st, MSG(timelft), (when / 60) + ((when % 60) ? 1 : 0));

  tmp = stream_copy(&st);
  net_write(USER(cp, who)->sd, tmp, stream_length(&st));
  free(tmp);
  stream_clear(&st);
}

void cmd_left(con *cp, int who, int *flag, char *rest) {
  int ix, jx, spc = 0;
  char *msg;
  user *p;

  if (cp->nml == 0) {
    net_write(USER(cp, who)->sd, XMSG(noleft));
  } else {
    stream st;
    char *tmp;
    time_t up = cp->up;
    struct tm *dtr;

    dtr = localtime(&up);

    stream_init(&st, 32);
    stream_printf(&st, MSG(upat),
                  ((dtr->tm_hour <= 12) ? dtr->tm_hour : dtr->tm_hour - 12),
                  dtr->tm_min, dtr->tm_sec,
                  ((dtr->tm_hour < 12) ? MSG(antemer) : MSG(postmer)));
    stream_printf(&st, MSG(leftmsg), cp->nml);

    msg = MSG(lefthdr);
    while (msg[spc] != ' ') ++spc;

    if (spc > 5) {
      spc -= 5;
    } else {
      spc = 5 - spc;
      for (ix = 0; ix < spc; ix++) stream_putch(&st, ' ');
      spc = 0;
    }
    stream_write(&st, msg, 0);

#ifdef DEBUG
    fprintf(stderr, "cmd_left: nml=%d left=%p\n", cp->nml, cp->left);
#endif
    for (ix = 0; ix < cp->nml; ix++) {
      if ((p = cp->left[ix]) != NULL) {
        up = p->when;
        dtr = localtime(&up);

#ifdef DEBUG
        fprintf(stderr, "cmd_left: ix=%d p=%p name=%s\n", ix, p, p->name);
#endif
        for (jx = 0; jx < spc; jx++) stream_putch(&st, ' ');
        stream_printf(&st, "%02d:%02d%c %s\r\n",
                      ((dtr->tm_hour <= 12) ? dtr->tm_hour : dtr->tm_hour - 12),
                      dtr->tm_min, (WASKILLED(p) ? 'K' : ' '), p->name);
      }
    }

    tmp = stream_copy(&st);
    net_write(USER(cp, who)->sd, tmp, stream_length(&st));

    stream_clear(&st);
    free(tmp);
  }
}

void cmd_dw(con *cp, int who, int *flag, char *rest) {
  if (cp->warn == NULL) {
    net_write(USER(cp, who)->sd, "\r\n\a\a\r\n", 4);
  } else {
    char *tmp;

    tmp = stream_copy(MSTR(cp->warn));
    net_write(USER(cp, who)->sd, tmp, stream_length(MSTR(cp->warn)));
    free(tmp);
  }
}

void cmd_clock(con *cp, int who, int *flag, char *rest) {
  char *tmp;
  time_t now = time(NULL);
  struct tm *dtr;
  stream st;

  dtr = localtime(&now);

  stream_init(&st, 32);
  stream_write(&st, "\r\n", 2);
  stream_printf(&st, MSG(clkmsg),
                (dtr->tm_hour <= 12) ? dtr->tm_hour : dtr->tm_hour - 12,
                dtr->tm_min, dtr->tm_sec,
                (dtr->tm_hour < 12) ? MSG(antemer) : MSG(postmer));
  tmp = stream_copy(&st);
  stream_clear(&st);

  net_write(USER(cp, who)->sd, tmp, strlen(tmp));

  free(tmp);
}

void cmd_usrs(con *cp, int who, int *flag, char *rest) {
  int ix, nu = 0;
  stream st;
  char *tmp;

  for (ix = 0; ix < cp->size; ix++) {
    if (USER(cp, ix) != NULL) ++nu;
  }

  stream_init(&st, 16);
  stream_printf(&st, MSG(numusrs), nu);
  tmp = stream_copy(&st);
  stream_clear(&st);

  net_write(USER(cp, who)->sd, tmp, strlen(tmp));

  free(tmp);
}

void cmd_all(con *cp, int who, int *flag, char *rest) {
  int fmt = FMT_STAT | FMT_PORT | FMT_NAME | FMT_ALL;

  if (ISRP(USER(cp, who)) && !ISMASTER(USER(cp, who)) &&
      !ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(noport));
    return;
  }

  cmd_who_raw(cp, who, &fmt, rest);
}

void cmd_tty(con *cp, int who, int *flag, char *rest) {
  stream st;
  char *tmp;

  stream_init(&st, 32);
  cmd_who_entry(cp, who, who, &st, FMT_STAT | FMT_NAME);
  tmp = stream_copy(&st);
  net_write(USER(cp, who)->sd, tmp, stream_length(&st));

  stream_clear(&st);
  free(tmp);
}

void cmd_ru(con *cp, int who, int *flag, char *rest) {
  struct rusage res;
  struct rlimit lim;
  stream st;
  char *tmp;

#ifdef DEBUG
  fprintf(stderr, "cmd_ru: entry\n");
#endif
  stream_init(&st, 32);
  getrusage(RUSAGE_SELF, &res); /* How much time have we used? */
  getrlimit(RLIMIT_CPU, &lim);  /* How much are we allowed?    */

#ifdef DEBUG
  fprintf(stderr, "cmd_ru: building output %.3f %u\n",
          (double)(res.ru_utime.tv_sec), (unsigned)(lim.rlim_cur));
#endif
  stream_printf(&st, MSG(crunow), (double)(res.ru_utime.tv_sec),
                (unsigned)(lim.rlim_cur));
  tmp = stream_copy(&st);

#ifdef DEBUG
  fprintf(stderr, "cmd_ru: writing '%s' to user\n", tmp);
#endif

  net_write(USER(cp, who)->sd, tmp, stream_length(&st));

  stream_clear(&st);
  free(tmp);

#ifdef DEBUG
  fprintf(stderr, "cmd_ru: exit\n");
#endif

} /* end cmd_ru() */

void cmd_below(con *cp, int who, int *flag, char *rest) {
  if (!ISMASTER(USER(cp, who)) && !ISSUBMASTER(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
  } else {
    int fmt = FMT_STAT | FMT_NAME | FMT_BELOW;

    cmd_who_raw(cp, who, &fmt, rest);
  }
}

/* --- Master commands */

void cmd_warn(con *cp, int who, int *flag, char *rest) {
  int ix = 0;

  if (!ISMASTER(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  while (isspace(rest[ix])) ++ix;

  if (rest[ix] == '\0') {
    net_write(USER(cp, who)->sd, XMSG(newwarn));
    SETSTATE(USER(cp, who), WARN_ST);
  } else {
    con_warn(cp, rest + ix);
  }
}

void cmd_kill(con *cp, int who, int *flag, char *rest) {
  byte *map;
  int ix, ne;

  if (!ISMASTER(USER(cp, who)) && !ISSUBMASTER(USER(cp, who)) &&
      !ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  map = calloc(MAPSIZE(cp), sizeof(byte));

  ne = cmd_argmap(cp, who, map, rest);
  if (ne >= 0) {
    if (MAPBIT(map, who)) {
      net_write(USER(cp, who)->sd, XMSG(invarg));
      free(map);
      return;
    }
    for (ix = 0; ix < cp->max; ix++) {
      if (!ne || MAPBIT(map, ix)) {
#ifdef DEBUG
        fprintf(stderr, "cmd_kill: ix = %d\n", ix);
#endif
        /* Submasters can only kill their own users; masters or priv
           users can kill anyone */
        if (ix == who) {
          continue;
        }
        if (USER(cp, ix)->mstr == who) {
          con_exuser(cp, ix, KILLED);
        } else if (ISMASTER(USER(cp, who)) || ISPRIV(USER(cp, who))) {
          con_exuser(cp, ix, KILLED);
        } else {
          net_write(USER(cp, who)->sd, XMSG(invarg));
          free(map);
          return;
        }
      }
    }
    net_write(USER(cp, who)->sd, XMSG(donemsg));
  }

  free(map);
}

void cmd_en(con *cp, int who, int *flag, char *rest) {
  if (ISMASTER(USER(cp, who)) || ISSUBMASTER(USER(cp, who))) {
    SETTELLALL(USER(cp, who));
    net_write(USER(cp, who)->sd, XMSG(donemsg));
  } else {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
  }
}

void cmd_dis(con *cp, int who, int *flag, char *rest) {
  if (ISMASTER(USER(cp, who)) || ISSUBMASTER(USER(cp, who))) {
    CLRTELLALL(USER(cp, who));
    net_write(USER(cp, who)->sd, XMSG(donemsg));
  } else {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
  }
}

void cmd_mty(con *cp, int who, int *flag, char *rest) {
  byte *map;
  int ix, ne;

  if (!ISMASTER(USER(cp, who)) && !ISSUBMASTER(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  map = calloc(MAPSIZE(cp), sizeof(byte));

  ne = cmd_argmap(cp, who, map, rest);
  if (ne < 0) {
    free(map);
    return;
  }

  if (ne == 0 || MAPBIT(map, who)) {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    free(map);
    return;
  }

  for (ix = 0; ix < cp->size; ix++) {
    if (MAPBIT(map, ix)) {
      if (USER(cp, ix)->mstr != who) {
        net_write(USER(cp, who)->sd, XMSG(invarg));
        free(map);
        return;
      }
      if (!ISSUBMASTER(USER(cp, ix))) {
        mptr mp = msg_alloc(strlen(MSG(nowmast)));

        msg_from(mp, who);
        msg_rcpt(mp, ix);
        msg_type(mp, M_NOTIFY);
        msg_write(mp, MSG(nowmast));

        SETSUBMASTER(USER(cp, ix));
        user_enqueue(USER(cp, ix), mp);
        msg_clear(mp);
      }
    }
  }

  net_write(USER(cp, who)->sd, XMSG(donemsg));
  free(map);
}

void cmd_norm(con *cp, int who, int *flag, char *rest) {
  byte *map;
  int ix, ne;

  if (!ISMASTER(USER(cp, who)) && !ISSUBMASTER(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  map = calloc(MAPSIZE(cp), sizeof(byte));

  ne = cmd_argmap(cp, who, map, rest);
  if (ne < 0) {
    free(map);
    return;
  }

  if (MAPBIT(map, who)) {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    free(map);
    return;
  }

  if (ne == 0) {
    for (ix = 0; ix < cp->size; ix++) {
      if (USER(cp, ix) != NULL && USER(cp, ix)->mstr == who &&
          ISSUBMASTER(USER(cp, ix)))
        SETMAP(map, ix);
    }
  }

  for (ix = 0; ix < cp->size; ix++) {
    if (MAPBIT(map, ix)) {
      if (USER(cp, ix)->mstr != who) {
        net_write(USER(cp, who)->sd, XMSG(invarg));
        free(map);
        return;
      }
      if (ISSUBMASTER(USER(cp, ix))) {
        mptr mp = msg_alloc(strlen(MSG(normal)));

        msg_from(mp, who);
        msg_rcpt(mp, ix);
        msg_write(mp, MSG(normal));

        CLRSUBMASTER(USER(cp, ix));
        user_enqueue(USER(cp, ix), mp);
        msg_clear(mp);

        con_normalize(cp, ix);
      }
    }
  }

  net_write(USER(cp, who)->sd, XMSG(donemsg));
  free(map);
}

void cmd_give(con *cp, int who, int *flag, char *rest) {
  int ix = 0, pos = 0;
  int to = 0;

  if (!ISMASTER(USER(cp, who)) && !ISSUBMASTER(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  /* Skip leading white space   */
  while (rest[pos] && isspace(rest[pos])) ++pos;

  /* Make sure we have a number */
  if (!isdigit(rest[pos])) {
    net_write(USER(cp, who)->sd, XMSG(fmterr));
    return;
  }

  /* Now, who to give to?       */
  while (isdigit(rest[pos])) {
    to = (to * 10) + (rest[pos++] - '0');
  }

  /* Is that a valid person?    */
  if (USER(cp, to) == NULL) {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    return;
  } else if (!ISMASTER(USER(cp, to)) && !ISSUBMASTER(USER(cp, to))) {
    net_write(USER(cp, who)->sd, XMSG(mptmmsg));
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "cmd_give: recipient is #%d: %s\n", to, USER(cp, to)->name);
#endif

  while (rest[pos] && isspace(rest[pos])) ++pos;

  /* Verify we have a semicolon */
  if (rest[pos] != ';') {
    net_write(USER(cp, who)->sd, XMSG(fmterr));
    return;
  }

  ++pos;
  while (rest[pos] && isspace(rest[pos])) ++pos;

  /*
    Okay -- the parameter has to be either 'new', or a list of numbers.
    You can't have both (I'm not sure if the original permitted both
    anyway, but this version surely does not :)
   */
  if (cmd_comp(rest + pos, "new")) {
    mptr mp;

    /* You can't pass new users if you don't have new users */
    if (cp->new != who) {
      net_write(USER(cp, who)->sd, XMSG(invarg));
      return;
    }

    cp->new = to;

    /* Notify the submaster of the change */
    mp = msg_alloc(strlen(MSG(passed)));
    msg_type(mp, M_NOTIFY);
    msg_write(mp, MSG(passed));
    msg_write(mp, MSG(pnumsg));

    user_enqueue(USER(cp, to), mp);
    msg_clear(mp);

    net_write(USER(cp, who)->sd, XMSG(donemsg));
    return;

  } else {
    byte *map;
    int ne;
    /*
      If we get here, then the parameter list should be a list
      of numbers, comma delimited, of users to be transferred.
      We'll make up the bitmap, and pass it off to con_give_map()
      to do the actual transfer and notification
     */
    map = calloc(MAPSIZE(cp), sizeof(byte));
    ne = cmd_argmap(cp, who, map, rest + pos);
    if (ne < 0) {
      free(map);
      return;
    } else if (ne == 0) {
      net_write(USER(cp, who)->sd, XMSG(fmterr));
      free(map);
      return;
    }

    /* Verify the entries in the bitmap */
    for (ix = 0; ix < cp->size; ix++) {
      if (MAPBIT(map, ix)) {
#ifdef DEBUG
        fprintf(stderr, "cmd_give: user #%d is set in the bitmap\n", ix);
#endif
        /* You can't give yourself, or someone else's users, away */
        if (ix == who || USER(cp, ix)->mstr != who) {
          net_write(USER(cp, who)->sd, XMSG(invarg));
          free(map);
          return;
        }
      }
    }

    /* Okay, they're all good -- do it! */
    con_give_map(cp, to, map);
    net_write(USER(cp, who)->sd, XMSG(donemsg));
  }
}

void cmd_nimp(con *cp, int who, int *flag, char *rest) {
  net_write(USER(cp, who)->sd, XMSG(unimp));
  SETSTATE(USER(cp, who), IDLE_ST);
}

/* --- Extended Xcaliber II commands */

/*
  This is like the 'all' command, but it is accessible only to masters
  and privileged users, and it returns everything, including the
  person's network address (even if they're rejecting ports)
 */
void cmd_xwho(con *cp, int who, int *flag, char *rest) {
  if (!ISMASTER(USER(cp, who)) && !ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
  } else {
    int fmt = FMT_STAT | FMT_PORT | FMT_NAME | FMT_LORD | FMT_ALL;

    cmd_who_raw(cp, who, &fmt, rest);
  }
}

/* Displays the language string for the currently-installed language */
void cmd_xdl(con *cp, int who, int *flag, char *rest) {
  stream st;
  char *tmp;

  if (!ISMASTER(USER(cp, who)) && !ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  stream_init(&st, 32);
  stream_printf(&st, MSG(curlang), g_lang);
  tmp = stream_copy(&st);

  net_write(USER(cp, who)->sd, tmp, stream_length(&st));

  stream_clear(&st);
  free(tmp);
}

/*
  This command is usable only by the main con master or a privileged
  user.  Basically, it sets the current language environment (which
  set of strings gets displayed for various messages in the
  conference).  The 'xdl' command can be used to display which
  language is currently in effect.
 */
void cmd_xlang(con *cp, int who, int *flag, char *rest) {
  int ix = 0;

  if (!ISMASTER(USER(cp, who)) && !ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  while (rest[ix] && isspace(rest[ix])) ++ix;

  if (rest[ix] == '\0') {
    net_write(USER(cp, who)->sd, XMSG(invarg));
    return;
  }

  if (lang_install(rest + ix)) {
    strcpy(g_lang, rest + ix);
    fprintf(stderr, "%s: changed language environment to '%s'\n", g_prog,
            g_lang);
    net_write(USER(cp, who)->sd, XMSG(donemsg));
  } else {
    net_write(USER(cp, who)->sd, XMSG(nolang));
  }
}

/*
  This is the interface to the privilege system.  The xyzzy command
  (so named for reasons obvious to old Adventure players) has two
  formats:
    xyzzy name;password
    xyzzy

  In the first format, it enables privileges for a given username and
  password combination.  These are defined in an external file which
  is read by Xcaliber (usually /usr/local/lib/xcal/.priv).  If the
  name and password match someone valid in that file, the priv bit for
  the user issuing the xyzzy command is turned on.

  The second form disables the priv bit for a user.
 */
void cmd_xyzzy(con *cp, int who, int *flag, char *rest) {
  int ix = 0, sc;

  if (ISPRIV(USER(cp, who))) {
    CLRPRIV(USER(cp, who));
    fprintf(stderr, "%s: privileges disabled for #%d\n", g_prog, who);
    net_write(USER(cp, who)->sd, XMSG(donemsg));
    return;
  }

  /* Skip whitespace....  */
  while (rest[ix] && isspace(rest[ix])) ++ix;

  if (!rest[ix]) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  /*
    At this point, ix is the index of the first non-whitespace character
    in the command line after the command.  Scan ahead for a semicolon
    (which there must be, or it is an error), replace that with a NUL,
    and pass both to the validator
   */
  sc = ix + 1;
  while (rest[sc] && rest[sc] != ';') ++sc;

  if (rest[sc] != ';') {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  rest[sc++] = '\0';

  if (val_check(rest + ix, rest + sc)) {
    SETPRIV(USER(cp, who));
    fprintf(stderr, "%s: privileges enabled for #%d as '%s'\n", g_prog, who,
            rest + ix);
    net_write(USER(cp, who)->sd, XMSG(donemsg));
  } else {
    net_write(USER(cp, who)->sd, XMSG(fmterr));
  }
}

/*
  This command extends the life of a conference.  It is only available
  to privileged users.  Basically, it takes a single argument which is
  a value of time in seconds.  It adds this many seconds to the time
  left before the conference expires.  Thus, for example:

    xtend 60

  ...extends the conference by 1 minute (60 seconds).
 */
void cmd_xtend(con *cp, int who, int *flag, char *rest) {
  int ix = 0;

  if (!ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  while (rest[ix] && isspace(rest[ix])) ++ix;

  if (isdigit(rest[ix])) {
    int val;

    /* Extend the life of the con */
    val = atoi(rest + ix);
    cp->til += (time_t)val;

    net_write(USER(cp, who)->sd, XMSG(donemsg));
  } else {
    net_write(USER(cp, who)->sd, XMSG(fmterr));
  }
}

/*
  Under unusual circumstances, the conference master can get
  disconnected without the server noticing.  This can be a pain in the
  butt, since the conference does not end if the main master is
  killed.  A privileged user, therefore, can use xreset to cause the
  conference to terminate immediately.  This basically terminates the
  conference in exactly the same way as the master disconnecting.
 */
void cmd_xreset(con *cp, int who, int *flag, char *rest) {
  if (!ISPRIV(USER(cp, who))) {
    net_write(USER(cp, who)->sd, XMSG(cmderr));
    return;
  }

  net_write(USER(cp, who)->sd, XMSG(donemsg));
  fprintf(stderr, "%s: conference reset by privileged user #%d (%s).\n", g_prog,
          who, USER(cp, who)->name);
  *flag = 1;
}

/*------------------------------------------------------------------------*/
/* HERE ENDETH THE COMMAND HANDLERS                                       */
/*------------------------------------------------------------------------*/

/* --- Static function definitions */

int cmd_match(char *str) {
  int ix = 0;

  while (cmdtab[ix].entry != 0) {
    if (cmd_comp(str, cmdtab[ix].name)) return ix;
    ++ix;
  }

  return -1;
}

int cmd_comp(char *str, char *cmd) {
  int sx = 0, cx = 0;
  int ok = 0;

  while (str[sx] && cmd[cx]) {
    if (tolower(str[sx]) == tolower(cmd[cx])) {
      ++sx;
      ++cx;
      ok = 0;
    } else if (cmd[cx] == '*') {
      ok = 1;
      ++cx;
    } else {
      return 0;
    }
  }

  if (str[sx] == '\0' && cmd[cx] == '\0')
    return 1;
  else if (str[sx] == '\0' && (ok || cmd[cx] == '*'))
    return 1;
  else
    return 0;
}

void cmd_who_map(con *cp, int who, byte *map, stream *sp, int format) {
  int ix;

  for (ix = 0; ix < cp->size; ix++) {
    if (MAPBIT(map, ix) && USER(cp, ix) != NULL) {
      cmd_who_entry(cp, ix, who, sp, format);
    }
  }
}

void cmd_who_entry(con *cp, int who, int fbo, stream *sp, int format) {
  int ix;

  if (who == cp->new)
    stream_write(sp, "=>", 2);
  else
    stream_write(sp, "  ", 2);

  if (format & FMT_LORD) {
    stream_printf(sp, "[#%d]%c%c ", who, (ISPRIV(USER(cp, who)) ? 'U' : ' '),
                  ((ISMASTER(USER(cp, who)) || ISSUBMASTER(USER(cp, who)))
                       ? 'M'
                       : USER(cp, who)->mstr + '0'));
  } else if (format & FMT_STAT) {
    stream_printf(
        sp, "[#%d] %c ", who,
        ((ISMASTER(USER(cp, who)) || ISSUBMASTER(USER(cp, who))) ? 'M' : ' '));
  } else {
    stream_printf(sp, "[#%d]  ", who);
  }

  if (format & FMT_STAT) {
    switch (STATE(USER(cp, who))) {
      case BUILD_ST:
        if (msg_toall(USER(cp, who)->inc))
          stream_write(sp, "B/A", 3);
        else if (msg_numrcpt(USER(cp, who)->inc) > 1)
          stream_write(sp, "B/P", 3);
        else {
          for (ix = 0; ix < cp->size; ix++)
            if (msg_isrcpt(USER(cp, who)->inc, ix)) break;
          if (USER(cp, ix) == NULL || USER(cp, ix)->sd < 0)
            stream_write(sp, "B/D", 0);
          else
            stream_printf(sp, "B-%d", ix);
        }
        break;

      case RECV_ST:
        stream_write(sp, "R/M", 3);
        break;

      case CMD_ST:
        stream_write(sp, "COM", 3);
        break;

      case LOGIN_ST:
        stream_write(sp, "E/N", 3);
        break;

      case CHG_ST:
        stream_write(sp, "C/N", 3);
        break;

      case OUT_ST:
        stream_write(sp, "OUT", 3);
        break;

      case LINE_ST:
        stream_write(sp, "LIN", 3);
        break;

      case READI_ST:
        stream_write(sp, "R/I", 3);
        break;

      case EXPL_ST:
        stream_write(sp, "EXP", 3);
        break;

      default:
        stream_write(sp, "---", 3);
        break;

    } /* end switch(state) */

    /* Status bits */
    stream_printf(sp, " %c%c%c%c%c  ", (ISRA(USER(cp, who)) ? 'R' : ' '),
                  (user_igging(USER(cp, who), fbo)
                       ? (user_igging(USER(cp, fbo), who) ? 'B' : 'X')
                       : (user_igging(USER(cp, fbo), who) ? 'I' : ' ')),
                  (ISRP(USER(cp, who)) ? 'P' : ' '),
                  (ISRC(USER(cp, who)) ? 'C' : ' '),
                  (ISRN(USER(cp, who)) ? ' ' : 'N'));
  } /* end if(FMT_STAT) */

  if (format & FMT_PORT) {
    if (ISRP(USER(cp, who)) && !(format & FMT_LORD)) {
      stream_printf(sp, "Rejecting ports ");
    } else {
      stream_printf(
          sp, "%8s %d/%04d ", map_port(USER(cp, who)->ip, USER(cp, who)->port),
          (USER(cp, who)->port) & 0x07, ((USER(cp, who)->ip & 0xFFFF) % 10000));
    }

  } /* end if(FMT_PORT) */

  if (format & FMT_LORD) {
    stream_printf(sp, "%d.%d.%d.%-3d %-5u ", (USER(cp, who)->ip >> 24) & 0xFF,
                  (USER(cp, who)->ip >> 16) & 0xFF,
                  (USER(cp, who)->ip >> 8) & 0xFF, USER(cp, who)->ip & 0xFF,
                  USER(cp, who)->port);
  } /* end if(FMT_LORD) */

  if (format & FMT_NAME) {
    if (STATE(USER(cp, who)) == LOGIN_ST)
      stream_printf(sp, "ENTERING NAME\r\n");
    else
      stream_printf(sp, "%s\r\n", USER(cp, who)->name);
  } else {
    stream_write(sp, "\r\n", 2);
  } /* end if(FMT_NAME) */
}

int cmd_argmap(con *cp, int who, byte *map, char *cmd) {
  int ix = 0, val, nf = 0;

  if (!cmd_isok(cmd)) {
    net_write(USER(cp, who)->sd, XMSG(badchar));
    return -1;
  }

#ifdef DEBUG
  fprintf(stderr, "cmd_argmap: start with cmd[%d] = '%c'\n", ix, cmd[ix]);
#endif
  while (cmd[ix] && isspace(cmd[ix])) ++ix;

  while (cmd[ix] && cmd[ix] != ';') {
    /* Skip leading whitspace */
    while (isspace(cmd[ix])) ++ix;

    /* Get digits, while possible */
    val = 0;
    while (isdigit(cmd[ix])) {
      val = (val * 10) + (cmd[ix] - '0');
      ++ix;
    }

    /* If we stopped because of a separator, okay, but otherwise
       this is an invalid argument */
#ifdef DEBUG
    fprintf(stderr, "cmd_argmap: stopped on cmd[%d] = '%c'\n", ix, cmd[ix]);
#endif
    if (!isspace(cmd[ix]) && cmd[ix] != ',' && cmd[ix] != ';' &&
        cmd[ix] != '\0') {
      net_write(USER(cp, who)->sd, XMSG(invarg));
      return -1;
    }

    /* Okay..so we have a valid argument.  Now check if it is in
       range, and if so, record it in the bitmap */
    if ((val < cp->size) && USER(cp, val) && USER(cp, val)->sd >= 0) {
#ifdef DEBUG
      fprintf(stderr, "cmd_argmap: match for id = %d\n", val);
#endif
      SETMAP(map, val);
      ++nf;
    } else {
      net_write(USER(cp, who)->sd, XMSG(invarg));
      return -1; /* But, alas, this one is too big */
    }

    if (cmd[ix] != '\0' && cmd[ix] != ';') ++ix;
  }

  return nf;
}

int cmd_isok(char *cmd) {
  int ix = 0;

  if (!cmd) return 0;

  while (cmd[ix] != '\0' && cmd[ix] != ';') {
    if (!isalpha(cmd[ix]) && !isdigit(cmd[ix]) && !isspace(cmd[ix]) &&
        cmd[ix] != ',' && cmd[ix] != ';') {
#ifdef DEBUG
      fprintf(stderr, "cmd_isok: bad character: 0x%02X '%c' offset %d\n",
              cmd[ix], cmd[ix], ix);
#endif
      return 0;
    }
    ++ix;
  }

  return 1;
}
