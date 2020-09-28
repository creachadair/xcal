#include <stdio.h>
#include <stdlib.h>

#include "port.h"
#include "stream.h"

int main(int argc, char *argv[]) {
  char *name;
  short port;
  stream st;

  stream_init(&st, 16);
  for (port = 0; port < 9000; port++) {
    name = map_port(port);
    stream_printf(&st, "0/%04d %8s|\n", port, name);
    name = stream_copy(&st);
    printf("%s", name);
    free(name);

    stream_flush(&st);
  }

  stream_clear(&st);
  return 0;
}
