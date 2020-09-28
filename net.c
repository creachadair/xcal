/*
    net.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Network utilities
 */

#include "net.h"

#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define BACKLOG 5

/*------------------------------------------------------------------------*/
int net_listen(short port) {
  int sd, opt = 1;
  addr ad;

  if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) return -1;

  /* Mark port reusable */
  setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

  ad.sin_family = AF_INET;
  ad.sin_port = htons(port);
  ad.sin_addr.s_addr = INADDR_ANY;

  if (bind(sd, (struct sockaddr *)&ad, sizeof(addr)) < 0) {
    close(sd);
    return -1;
  }

  if (listen(sd, BACKLOG) < 0) {
    close(sd);
    return -1;
  }

  return sd;
}

/*------------------------------------------------------------------------*/
int net_accept(int sd, int *ip, short *port) {
  unsigned len = sizeof(addr);
  int nd;
  addr in;

  if ((nd = accept(sd, (struct sockaddr *)&in, &len)) < 0) return -1;

  if (ip) *ip = ntohl(in.sin_addr.s_addr);

  if (port) *port = ntohs(in.sin_port);

  return nd;
}

/*------------------------------------------------------------------------*/
void net_close(int sd) { close(sd); }

/*------------------------------------------------------------------------*/
int net_connect(char *host, short port) {
  struct hostent *hent;
  int sd;
  addr to;

  if ((hent = gethostbyname(host)) == NULL) return -1;

  if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) return -1;

  to.sin_family = AF_INET;
  to.sin_port = htons(port);
  memcpy(&(to.sin_addr), hent->h_addr, hent->h_length);

  if (connect(sd, (struct sockaddr *)&to, sizeof(addr)) < 0) {
    close(sd);
    return -1;
  }

  return sd;
}

/*------------------------------------------------------------------------*/
int net_read(int sd, char *buf, int len) { return recv(sd, buf, len, 0); }

/*------------------------------------------------------------------------*/
int net_write(int sd, char *buf, int len) { return send(sd, buf, len, 0); }
