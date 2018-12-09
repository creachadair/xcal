/*
    net.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Network utilities
 */

#ifndef XCAL_NET_H_
#define XCAL_NET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct sockaddr_in   addr;

int    net_listen(short port);
int    net_accept(int sd, int *ip, short *port);
void   net_close(int sd);
int    net_connect(char *host, short port);

int    net_read(int sd, char *buf, int len);
int    net_write(int sd, char *buf, int len);

#endif /* end XCAL_NET_H_ */
