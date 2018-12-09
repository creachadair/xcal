/*
    log.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Connexion logging for Xcaliber Mark II
 */

#ifndef XCAL_LOG_H_
#define XCAL_LOG_H_

extern char *log_dir;
extern int   log_enabled;

void log_connect(int ip, short port, short id);
void log_disconnect(short id);
void log_kill(short id, short killer);
void log_startup(int up);

#endif /* end XCAL_LOG_H_ */
