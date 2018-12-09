#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "net.h"
#include "stream.h"

int main(int argc, char *argv[]) {
  int list;
  int fds[5];
  int next = 0, ix, nd, nr, done = 0;
  fd_set rds;
  char buf[10];
  stream str[5];

  for (ix = 0; ix < 5; ix++) {
    fds[ix] = -1;
    stream_init(str + ix, 16);
  }

  list = net_listen(2112);
  if (list < 0) {
    fprintf(stderr, "%s: net_listen: %s\n", argv[0], strerror(errno));
    return 1;
  }

  while (!done) {
    FD_ZERO(&rds);

    FD_SET(list, &rds);
    for (ix = 0; ix < next; ix++) {
      if (fds[ix] < 0) continue;
      FD_SET(fds[ix], &rds);
    }

    if ((nd = select(getdtablehi(), &rds, NULL, NULL, NULL)) < 0) {
      fprintf(stderr, "%s: select: %s\n", argv[0], strerror(errno));
      break;
    }

    fprintf(stderr, "%d descriptors have activity\n", nd);
    if (FD_ISSET(list, &rds)) {
      int ip;
      short port;

      fds[next++] = net_accept(list, &ip, &port);
      fprintf(stderr,
              "fd[%d] = %d got a new connexion from "
              "%d.%d.%d.%d port %d\n",
              next - 1, fds[next - 1], (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
              (ip >> 8) & 0xFF, ip & 0xFF, port);

      if (next >= 5) next = 0;
      if (fds[next] >= 0) {
        close(fds[next]);
        stream_flush(str + next);
      }
    }

    for (ix = 0; ix < next; ix++) {
      if (fds[ix] < 0) continue;

      if (FD_ISSET(fds[ix], &rds)) {
        fprintf(stderr, "fd[%d] = %d is readable\n", ix, fds[ix]);
        nr = recv(fds[ix], buf, 10, 0);

        if (nr < 0) {
          fprintf(stderr, "Aha! fd[%d] = %d error: %s\n", ix, fds[ix],
                  strerror(errno));
          close(fds[ix]);
          fds[ix] = -1;
          stream_flush(str + ix);
        } else if (nr == 0) {
          fprintf(stderr, "Lost connexion to fd[%d] = %d\n", ix, fds[ix]);
          close(fds[ix]);
          fds[ix] = -1;
          stream_flush(str + ix);
        } else {
          char *tmp;

          fprintf(stderr, "%d bytes read\n", nr);
          stream_write(str + ix, buf, nr);

          if ((tmp = stream_readln(str + ix)) != NULL) {
            fprintf(stderr, ">> %d: [%d] %s\n", ix, strlen(tmp), tmp);
            if (strcmp(tmp, "quit") == 0) {
              fprintf(stderr, "QUIT\n");
              done = 1;
              break;
            }
            free(tmp);
          }
        }
      }
    }
    fprintf(stderr, "done checking\n");
  }

  net_close(list);
  return 0;
}
