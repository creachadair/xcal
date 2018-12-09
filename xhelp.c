/*
    xhelp.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Help system for XCaliber Mark II
 */

#include "xhelp.h"
#include "cmd.h"

#include <stdlib.h>

#include "default.h"

#ifdef PATH_MAX
#define PATHLEN PATH_MAX
#else
#define PATHLEN DEFAULT_PATHLEN
#endif

#define HELPDIR_ENV "XCALHELPDIR"

char *help_dir = DEFAULT_HELP_DIR; /* from default.h */

/*
  The table file is a binary file designed to be easy to scan from a C
  program.  The first three bytes of the file are the characters
  'XM2', followed by zero or more topic-to-filename mappings, and
  terminated by two binary 0xFF's.

  A topic-to-filename mapping maps topic strings to the names of the
  files that contain the help text for those topics.  These files are
  assumed to be in the directory whose name is pointed to by the
  global help_dir.  But if the environment variable XCALHELPDIR is
  defined, we use that instead.

  The mappings consist of a filename (a length byte plus the bytes of
  the name), a one-byte topic-count, and a sequence of that many topic
  strings (each consisting of a length byte plus the bytes of the
  topic).  This list is followed by a binary 0xFF, and then the next
  mapping, if any, in the file.
 */

static int help_check(FILE *fp);
static int help_match(char *t1, char *t2);
static FILE *help_open(char *fn, char *mode);

int help_lookup(char *toc, char *topic, FILE **fp) {
  FILE *tfp;
  int ch1, ch2, ix;
  char fbuf[80], tbuf[80];

  if ((tfp = help_open(toc, "rb")) == NULL)
    return HELP_TFNF; /* table file not found      */

  if (!help_check(tfp)) return HELP_FMT; /* table file has bad format */

  while ((ch1 = fgetc(tfp)) != EOF) {
    /* Check for end of table */
    if (ch1 == 0xFF) {
      if ((ch2 = fgetc(tfp)) == 0xFF) {
        break;
      } else {
        ungetc(ch2, tfp);
      }
    }
    fread(fbuf, sizeof(char), ch1, tfp); /* grab filename */
    fbuf[ch1] = '\0';

    ch1 = fgetc(tfp); /* get number of topic strings here  */
    for (ix = 0; ix < ch1; ix++) {
      ch2 = fgetc(tfp);
      fread(tbuf, sizeof(char), ch2, tfp); /* grab topic string */
      tbuf[ch2] = '\0'; /* add a terminator like good children  */

      if (help_match(topic, tbuf)) {
#ifdef DEBUG
        fprintf(stderr, "help_lookup: user string '%s' matches '%s'\n", topic,
                tbuf);
#endif
        fclose(tfp);

        if ((tfp = help_open(fbuf, "r")) == NULL) return HELP_HFNF;

        *fp = tfp;
        return HELP_OK;
      }
    }

    ch1 = fgetc(tfp); /* suck up trailing 0xFF */
  }                   /* end of while(!eof) */

  fclose(tfp);
  return HELP_NF;
}

int help_check(FILE *fp) {
  char buf[3];

  if (fread(buf, sizeof(char), 3, fp) < 3) return 0;

  if (buf[0] != 'X' || buf[1] != 'M' || buf[2] != '2')
    return 0;
  else
    return 1;
}

int help_match(char *t1, char *t2) { return cmd_comp(t1, t2); }

FILE *help_open(char *fn, char *mode) {
  char nbuf[PATHLEN];
  char *tmp;
  FILE *fp;

  if ((tmp = getenv(HELPDIR_ENV)) == NULL) tmp = help_dir;

  sprintf(nbuf, "%s/%s", tmp, fn);
#ifdef DEBUG
  fprintf(stderr, "help_open: opening file %s\n", nbuf);
#endif
  if ((fp = fopen(nbuf, mode)) == NULL) {
#ifdef DEBUG
    fprintf(stderr, "help_open: failed\n");
#endif
    return NULL;
  } else {
#ifdef DEBUG
    fprintf(stderr, "help_open: okay\n");
#endif
    return fp;
  }
}
