#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mktab(FILE *ifp, FILE *ofp);

int main(int argc, char *argv[]) {
  FILE *fp;

  if (argc < 2) {
    fprintf(stderr, "%s: usage is '%s filename'\n", argv[0], argv[0]);
    return 1;
  }

  fp = fopen(argv[1], "r");
  if (!fp) {
    fprintf(stderr, "%s: can't open file '%s' for reading\n", argv[0], argv[1]);
    return 1;
  }

  mktab(fp, stdout);
  fclose(fp);
  return 0;
}

void mktab(FILE *ifp, FILE *ofp) {
  int ch, ix, ls = 1, ntop = 0;
  long pos = 0;
  char cbuf[64];

  while ((ch = fgetc(ifp)) != EOF) {
    if (ch == '*' && ls) {
      ix = 0;
      while ((ch = fgetc(ifp)) != '\n') {
        cbuf[ix++] = ch;
      }
      cbuf[ix] = '\0';
      pos += ix;
      fprintf(ofp, "%04ld %s\n", pos + 2, cbuf);
      ++ntop;
    } else if (ch == '\n') {
      ls = 1;
    } else {
      ls = 0;
    }
    fprintf(ofp, "%d topics\n", ntop);
    ++pos;
  }
}
