#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_comp(char *str, char *cmd);

int main(void) {
  char str[64];
  char cmd[64];

  while (1) {
    printf("Str: ");
    if (fgets(str, 64, stdin) == NULL) break;
    str[strlen(str) - 1] = '\0';

    printf("Cmd: ");
    if (fgets(cmd, 64, stdin) == NULL) break;
    cmd[strlen(cmd) - 1] = '\0';

    if (cmd_comp(str, cmd))
      printf("Match\n");
    else
      printf("No match\n");
  }

  return 0;
}

int cmd_comp(char *str, char *cmd) {
  int sx = 0, cx = 0;
  int ok = 0;

  fprintf(stderr, "str = %s, cmd = %s\n", str, cmd);

  while (str[sx] && cmd[cx]) {
    fprintf(stderr, "str[%d] = %c, cmd[%d] = %c, ok = %d\n", sx, str[sx], cx,
            cmd[cx], ok);
    if (str[sx] == cmd[cx]) {
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

  if (str[sx] == '\0' && cmd[cx] == '\0') {
    fprintf(stderr, "Matched normally\n");
    return 1;
  } else if (str[sx] == '\0' && (ok || cmd[cx] == '*')) {
    fprintf(stderr, "Matched special case\n");
    return 1;
  } else {
    return 0;
  }
}
