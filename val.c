/*
    val.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Validation routines for Xcaliber Mark II
 */

#include "val.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* for crypt(3) */

#include "default.h"

#ifdef PATH_MAX
#define PATHLEN PATH_MAX
#else
#define PATHLEN DEFAULT_PATHLEN
#endif

#define PRIVDIR_ENV "XCALPRIVDIR"

char *val_dir = DEFAULT_PRIV_DIR; /* from default.h */

FILE *val_open(char *fn, char *mode);

int val_check(char *id, char *pw) {
  FILE *fp;
  int res = 0, pos;
  char buf[80], salt[3], *crpw;

  if ((fp = val_open("priv", "r")) == NULL) return 0;

  /*
    Scan over each entry until the user is found, or until the file
    is exhausted
   */
  while (fgets(buf, 80, fp) != NULL) {
    buf[strlen(buf) - 1] = '\0'; /* clip trailing newline */

    pos = 0;
    while (buf[pos] && buf[pos] != ':') ++pos;

    if (buf[pos] == '\0') continue;

    buf[pos++] = '\0';

#ifdef DEBUG
    fprintf(stderr, "val_check: name='%s' pw='%s'\n", buf, buf + pos);
#endif
    /*
      At this point, we have a valid line; does the user name match?
     */
    if (strcmp(id, buf) == 0) {
      salt[0] = buf[pos];
      salt[1] = buf[pos + 1];
      salt[2] = '\0';

      crpw = crypt(pw, salt);
#ifdef DEBUG
      fprintf(stderr, "val_check: user pw='%s'\n", pw);
#endif
      if (strcmp(crpw, buf + pos) == 0) {
        res = 1;
        break;
      } else {
        res = 0;
        break;
      }
    }
  }

  fclose(fp);
  return res;
}

/*------------------------------------------------------------------------*/
FILE *val_open(char *fn, char *mode) {
  char nbuf[PATHLEN];
  char *tmp;
  FILE *fp;

  if ((tmp = getenv(PRIVDIR_ENV)) == NULL) tmp = val_dir;

  sprintf(nbuf, "%s/%s", tmp, fn);
#ifdef DEBUG
  fprintf(stderr, "val_open: opening file %s\n", nbuf);
#endif
  if ((fp = fopen(nbuf, mode)) == NULL) {
#ifdef DEBUG
    fprintf(stderr, "val_open: failed\n");
#endif
    return NULL;
  } else {
#ifdef DEBUG
    fprintf(stderr, "val_open: okay\n");
#endif
    return fp;
  }
}
