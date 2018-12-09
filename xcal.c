/*
    xcal.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Main driver program for XCaliber Mark II
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>

#include "con.h"
#include "lang.h"
#include "net.h"

#include "default.h"

#define VERSION "1.32"
#define MAXUSERS DEFAULT_MAXUSERS
#define DPORT DEFAULT_PORT
#define RETRIES 24
#define SLEEPTIME 5

con g_con;                 /* this is the conference itself */
int g_quit = 0;            /* flag for signal aborts        */
int g_loop = 0;            /* flag for reset loop           */
char *g_prog = NULL;       /* program name for diagnostics  */
char *g_vers = VERSION;    /* version string for commands   */
char g_lang[16] = "en_us"; /* language string               */
short g_port = DPORT;      /* listener port                 */
int g_users = MAXUSERS;    /* maximum # of users per con    */
int g_conlength = 45;      /* conference length, in minutes */
int g_termwarn = 120;      /* how long before to warn folks */
int g_trblk = SENDBLK;     /* size of transfer block, bytes */

extern char *optarg;    /* from the getopt() package          */
extern char **lang_tab; /* from the language package          */
extern int log_enabled; /* from the logging package           */

void sig_int(int ign); /* interrupt signal handler, POSIX style */

int main(int argc, char *argv[]) {
  int opt, retry = RETRIES;

  g_prog = argv[0];

  while ((opt = getopt(argc, argv, "cd:e:lp:t:w:hv")) != EOF) {
    switch (opt) {
      case 'c': /* toggle conference logging         */
        log_enabled = !log_enabled;
        break;
      case 'd': /* conference duration, in minutes   */
        g_conlength = atoi(optarg);
        break;
      case 'e': /* set language environment          */
        strcpy(g_lang, optarg);
        break;
      case 'l': /* whether to reset after shutdown   */
        g_loop = 1;
        break;
      case 'p': /* what TCP port to listen on        */
        g_port = atoi(optarg);
        break;
      case 't': /* how many bytes to send per block  */
        g_trblk = atoi(optarg);
        break;
      case 'w': /* how long before shutdown to warn  */
        g_termwarn = atoi(optarg);
        break;
      case 'h': /* I think this speaks for itself    */
        fprintf(stderr,
                "The following command line options are available:\n"
                "  -c        : toggle conference logging (currently %s)\n"
                "  -d dur    : conference duration in minutes (default %d)\n"
                "  -e env    : set language environment (default %s)\n"
                "  -l        : loop, restarting conference after shutdown\n"
                "  -p port   : start conference at given port (default %d)\n"
                "  -t size   : transfer block size (default %d bytes)\n"
                "  -w warn   : warn timeout in seconds (default %d)\n"
                "  -v        : display version information\n"
                "  -h        : display this message again\n\n",
                (log_enabled ? "on" : "off"), g_conlength, g_lang, g_port,
                g_trblk, g_termwarn);
        return 0;
      case 'v':
        fprintf(stderr,
                "Xcaliber Mark II, version %s, by "
                "Michael J. Fromberger\nCopyright (C) 1997-1998 "
                "Michael J. Fromberger, All Rights Reserved\n\n"
                "Based on XCALIBER multi-terminal conference program\n"
                "by David D. Wright '78 Dartmouth College\n"
                "and Michael S. Morton '80 Dartmouth College\n\n",
                VERSION);
        return 0;
      default:
        fprintf(stderr, "Usage is: %s [-p port] [-hv]\n", g_prog);
        return 1;
    }
  }

  printf(
      "Welcome to Xcaliber Mark II, by Michael J. Fromberger\n"
      "Copyright (C) 1997-1998 Michael J. Fromberger, "
      "All Rights Reserved\n\n");
  fflush(stdout);

  /* If possible load the requested language environment */
  lang_reset();
  if (lang_install(g_lang)) {
    fprintf(stderr, "%s: using '%s' language environment\n", g_prog, g_lang);
  } else {
    fprintf(stderr,
            "%s: unable to load language table for '%s'\n"
            "%s: using default messages\n",
            g_prog, g_lang, g_prog);
  }

  /* Go into a loop starting and running conferences    */
  g_con.ear = 0;
  do {
    fprintf(stderr, "%s: starting up conference at port %d\n", g_prog, g_port);

    /* Loop waiting for the address to become free      */
    while (1) {
      con_init(&g_con, g_users, g_port, g_trblk, (!g_con.ear));
      if (g_con.ear < 0) {
        if (errno == EADDRINUSE && retry && !g_loop) {
          sleep(SLEEPTIME);
          --retry;
          continue;
        } else {
          fprintf(stderr, "%s: error: %s\n", g_prog, strerror(errno));
          con_clear(&g_con);
          return 1;
        }
      }
      break;
    }

    /*
      Now the conference is started, but has no users.  We don't want to
      run down the timer if nobody's connected, or on average people will
      only get half the time they so richly deserve.  Thus, we block on
      the accept of the first user until a master has connected, and then
      start the actual maintenance loop.  (See con.c for more info)

      While the con is running, we catch interrupts so we can shut down
      gracefully.  Also, we ignore broken pipes so that we can recover
      relatively happily from lost connexions.
     */
    signal(SIGINT, sig_int);
    signal(SIGPIPE, SIG_IGN); /* ignore pipes, write() returns errors */
    fprintf(stderr, "%s: waiting for conference master ... \n", g_prog);
    con_newuser(&g_con);

    fprintf(stderr, "%s: time limit: %d hr %d min\n", g_prog,
            (g_conlength / 60), (g_conlength % 60));

    con_run(&g_con, g_conlength);
    signal(SIGINT, SIG_DFL);

    /*
      After the master has exited, or an interrupt is received, the
      conference closes down, disconnecting all its users.  If we are
      in "looping" mode (using the '-l' command line option), we will
      simply 'reset' the conference -- free all its data structures
      and re-initialize, but without closing its listener.  This keeps
      us from having to spin out waiting for the TIME_WAIT state to
      finish if we were to close the socket
     */
    fprintf(stderr, "%s: conference has terminated\n", g_prog);
    con_shutdown(&g_con, (!g_loop || g_quit));
    signal(SIGPIPE, SIG_DFL);

    fprintf(stderr, "%s: all users have been disconnected\n", g_prog);
    fprintf(stderr, "---------------------------------------\n");
    if (g_loop && !g_quit) {
      fprintf(stderr, "%s: conference resetting ... \n", g_prog);
      con_reset(&g_con);
    } else {
      fprintf(stderr, "%s: conference closing down\n", g_prog);
      con_clear(&g_con);
    }
    retry = RETRIES; /* Re-set the time-wait retry count */
  } while (g_loop && !g_quit);

  lang_reset();

  fprintf(stderr, "%s: finished\n", g_prog);
  return 0;
}

void sig_int(int ign) {
  if (g_quit) {
    fprintf(stderr, "%s: shutting down, please be patient\n", g_prog);
  } else {
    fprintf(stderr, "%s: INTERRUPT: closing down conference\n", g_prog);
    net_close(g_con.ear);
    g_quit = 1;
  }
  signal(SIGINT, sig_int);
}
