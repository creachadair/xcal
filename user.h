/*
    user.h
  
    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    User connection record structure and ancillary routines for
    XCaliber Mark II
 */

#ifndef XCAL_USER_H_
#define XCAL_USER_H_

#include "message.h"
#include "stream.h"

#include <time.h>

typedef struct q {
  mptr       msg;
  struct q  *link;
} qelt;

typedef qelt   *qptr;

typedef unsigned short  word;

#define  NAMELEN  64        /* maximum name length                        */
#define  ALL      -1        /* bit for 'ignore all'                       */

#define  MASTER_FLG   0x8000  /* user is master of the con                */
#define  SUBMST_FLG   0x4000  /* user is a submaster                      */
#define  GETNEW_FLG   0x2000  /* master receives new users                */
#define  TELALL_FLG   0x1000  /* master's subconference permits tell-all  */
#define  REJALL_FLG   0x0800  /* user is rejecting tell-alls              */
#define  REJPRT_FLG   0x0400  /* user is rejecting ports                  */
#define  REJBRK_FLG   0x0200  /* user is rejecting breaks                 */
#define  REJNOT_FLG   0x0100  /* user is rejecting notifications          */
#define  REJCTL_FLG   0x0080  /* user is rejecting controls               */
#define  WASKLT_FLG   0x0040  /* user was killed                          */
#define  PRVBIT_FLG   0x0020  /* user has "special permissions"           */

#define  STATE_MASK   0x001F  /* mask for state information               */
#define  IDLE_ST      0       /* user is idle, doing nothing              */
#define  BUILD_ST     1       /* user is building a message               */
#define  DONE_ST      2       /* message is ready to be sent              */
#define  RECV_ST      3       /* user is receiving messages               */
#define  CMD_ST       4       /* user is sending a command                */
#define  LOGIN_ST     5       /* user is entering name (login)            */
#define  LOGGED_ST    6       /* user is logged in                        */
#define  CHG_ST       7       /* user is changing name                    */
#define  CHGD_ST      8       /* user is done changing name               */
#define  OUT_ST       9       /* user is "out", no messages               */
#define  WARN_ST      10      /* master is building a warning             */
#define  WARND_ST     11      /* master is done building warning          */
#define  READI_ST     12      /* user is reading instructions             */
#define  EXPL_ST      13      /* user is explaining something             */
#define  LINE_ST      14      /* like idle, but no messages!              */
#define  DEAD_ST      15      /* dead but not yet reaped from table       */

#define  SLASH_OPT    0x8000  /* vowel-slash translation option           */

typedef struct usr {
  int        sd;            /* user's network connection                  */
  short      port;          /* port they're connected from                */
  int        ip;            /* IP address they're connected from          */
  stream     in;            /* message data received so far               */

  qptr       mqt;           /* outgoing message queue (tail)              */
  qptr       mqh;           /* outgoing message queue (head)              */
  mptr       inc;           /* incoming message (if any)                  */
  stream     out;           /* outgoing message (if any)                  */
  
  char       name[NAMELEN]; /* user's handle (id)                         */
  word       bits;          /* flag bits                                  */
  word       opts;          /* option bits                                */
  word       mstr;          /* id of this user's master in global terms   */

  byte      *igmap;         /* users that are ignored at present          */
  int        iglen;         /* size of igmap, in bytes                    */

  time_t     when;          /* timestamp for connect/disconnect time      */
} user;

typedef user *uptr;

void  user_init(uptr up, int sz);
void  user_clear(uptr up);
int   user_poll(uptr up, int rd);

void  user_set_port(uptr up, short port);
void  user_set_ip(uptr up, int ip);
void  user_set_name(uptr up, char *name);
void  user_ignore(uptr up, int who);
void  user_accept(uptr up, int who);
int   user_igging(uptr up, int who);

void  user_start_msg(uptr up);
void  user_add_recipient(uptr up, int rcpt);
void  user_del_recipient(uptr up, int rcpt);
void  user_abort_msg(uptr up);
mptr  user_end_msg(uptr up);

int   user_handle_telnet(uptr up);

void  user_force(uptr up, mptr mp);
void  user_enqueue(uptr up, mptr mp);
mptr  user_dequeue(uptr up);

/* Macros for getting and setting 'bits' values */
#define ISMASTER(U)      ((U)->bits&MASTER_FLG)
#define ISSUBMASTER(U)   ((U)->bits&SUBMST_FLG)
#define GETSNEW(U)       ((U)->bits&GETNEW_FLG)
#define TELLALL(U)       ((U)->bits&TELALL_FLG)
#define ISRC(U)          ((U)->bits&REJCTL_FLG)
#define ISRA(U)          ((U)->bits&REJALL_FLG)
#define ISRP(U)          ((U)->bits&REJPRT_FLG)
#define ISRB(U)          ((U)->bits&REJBRK_FLG)
#define ISRN(U)          ((U)->bits&REJNOT_FLG)
#define WASKILLED(U)     ((U)->bits&WASKLT_FLG)
#define ISPRIV(U)        ((U)->bits&PRVBIT_FLG)

#define SETMASTER(U)     do {(U)->bits|=MASTER_FLG;} while(0)
#define SETSUBMASTER(U)  do {(U)->bits|=SUBMST_FLG;} while(0)
#define SETGETNEW(U)     do {(U)->bits|=GETNEW_FLG;} while(0)
#define SETTELLALL(U)    do {(U)->bits|=TELALL_FLG;} while(0)
#define SETRC(U)         do {(U)->bits|=REJCTL_FLG;} while(0)
#define SETRA(U)         do {(U)->bits|=REJALL_FLG;} while(0)
#define SETRP(U)         do {(U)->bits|=REJPRT_FLG;} while(0)
#define SETRB(U)         do {(U)->bits|=REJBRK_FLG;} while(0)
#define SETRN(U)         do {(U)->bits|=REJNOT_FLG;} while(0)
#define SETKILLED(U)     do {(U)->bits|=WASKLT_FLG;} while(0)
#define SETPRIV(U)       do {(U)->bits|=PRVBIT_FLG;} while(0)
#define CLRMASTER(U)     do {(U)->bits&=(~MASTER_FLG);} while(0)
#define CLRSUBMASTER(U)  do {(U)->bits&=(~SUBMST_FLG);} while(0)
#define CLRGETNEW(U)     do {(U)->bits&=(~GETNEW_FLG);} while(0)
#define CLRTELLALL(U)    do {(U)->bits&=(~TELALL_FLG);} while(0)
#define CLRRC(U)         do {(U)->bits&=(~REJCTL_FLG);} while(0)
#define CLRRA(U)         do {(U)->bits&=(~REJALL_FLG);} while(0)
#define CLRRP(U)         do {(U)->bits&=(~REJPRT_FLG);} while(0)
#define CLRRB(U)         do {(U)->bits&=(~REJBRK_FLG);} while(0)
#define CLRRN(U)         do {(U)->bits&=(~REJNOT_FLG);} while(0)
#define CLRKILLED(U)     do {(U)->bits&=(~WASKLT_FLG);} while(0)
#define CLRPRIV(U)       do {(U)->bits&=(~PRVBIT_FLG);} while(0)

#define STATE(U)         ((U)->bits&STATE_MASK)
#define SETSTATE(U,S)    do {(U)->bits&=(~STATE_MASK);\
                             (U)->bits|=((S)&STATE_MASK);}while(0)
#define ISIDLE(U)        (STATE(U)==IDLE_ST)
#define ISBUILDING(U)    (STATE(U)==BUILD_ST)
#define ISRECV(U)        (STATE(U)==RECV_ST)
#define ISCOM(U)         (STATE(U)==CMD_ST)
#define ISLOGIN(U)       (STATE(U)==LOGIN_ST)
#define ISOUT(U)         (STATE(U)==OUT_ST)
#define ISDEAD(U)        (STATE(U)==DEAD_ST)
#define ISALIVE(U)       ((U)!=NULL&&STATE((U))!=DEAD_ST)
#define RECEIVING(U)     (STATE(U)==RECV_ST||STATE(U)==EXPL_ST||\
			  STATE(U)==READI_ST)
#endif /* end XCAL_USER_H_ */
