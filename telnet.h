/*
    telnet.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Routines for handling telnet commands in the input stream
 */

#ifndef XCAL_TELNET_H_
#define XCAL_TELNET_H_

#include "net.h"
#include "stream.h"

/* ---- Telnet commands from RFC854 --------------------- */
#define IAC    0xFF      /* interpret-as-command          */
#define SE     0xF0      /* end of subnegotiation parms   */
#define NOP    0xF1      /* no operation                  */
#define BRK    0xF3      /* break character               */
#define INTR   0xF4      /* interrupt process             */
#define ABORT  0xF5      /* abort output                  */
#define POKE   0xF6      /* are you there?                */
#define ECHAR  0xF7      /* erase character               */
#define ELINE  0xF8      /* erase line                    */
#define GOAHD  0xF9      /* go ahead signal               */
#define SB     0xFA      /* begin subnegotiation phase    */
#define WILL   0xFB      /* confirm indicated option      */
#define WONT   0xFC      /* refuse indicated option       */
#define DO     0xFD      /* request indicated option      */
#define DONT   0xFE      /* request desist of option      */
#define DATAX  0xFF      /* data-byte 255                 */

/* ---- Telnet virtual terminal special characters ------ */
#define T_NUL  0
#define T_BEL  7
#define T_LF   10
#define T_CR   13
#define T_BS   8
#define T_HTAB 9
#define T_VTAB 11
#define T_FORM 12

int  tel_hasreq(stream *sp, char *req, char *opt);
void tel_send(int sd, char req, char opt);

/*------------------------------------------------------- */
/* ---- Telnet option codes, from various RFC's --------- */
/*------------------------------------------------------- */

#define TO_BINARY    0
#define TO_ECHO      1
#define TO_SUPPRESS  3


#endif /* end XCAL_TELNET_H_ */
