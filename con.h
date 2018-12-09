/*
    con.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Conference structures for Xcaliber Mark II
 */

#ifndef XCAL_CON_H_
#define XCAL_CON_H_

#include "message.h"
#include "user.h"

#include <time.h>

#define  USER(C,W)  ((C)->who[(W)])

#define  POST_OK    0    /* posted successfully    */
#define  POST_IG    1    /* user is ignoring you   */
#define  POST_OUT   2    /* user is out            */
#define  POST_DIS   3    /* user disconnected      */
#define  POST_NR    4    /* user not a recipient   */
#define  POST_SDR   5    /* user is sender         */

#define  DIED       0    /* connexion died or quit */
#define  KILLED     1    /* user was killed        */

#define  SENDBLK    512  /* message burst size in bytes */
#define  BOUNCETAB  8    /* length of bounce list  */

#include "lang.h"  /* fixed messages */

#define NEWCON_FLG   0x0001

#define ISNEWCON(C)  ((C)->bits&NEWCON_FLG)
#define SETNEWCON(C) do {(C)->bits|=NEWCON_FLG;} while(0)
#define CLRNEWCON(C) do {(C)->bits&=~NEWCON_FLG;} while(0)

typedef struct bo {
  int    ip;
  short  port;
  char   hide;
} bolist;

typedef struct {
  int    size;  /* maximum number of users permitted on conference */
  user **who;   /* pointers to user records                        */

  int    num;   /* number of users currently connected             */
  int    max;   /* maximum number of users ever seen so far        */
  int    nml;   /* number of users who have left                   */
  int    mnl;   /* maximum length of left list (so far)            */
  user **left;  /* pointers to records of left users               */

  int    ear;   /* listener socket, where new connexions arrive    */

  mptr   warn;  /* current warning, if any                         */
  word   bits;  /* flag bits and other administrivia               */

  time_t up;    /* time conference was raised                      */
  time_t til;   /* time conference will end                        */

  int    trblk; /* transmission block size, in bytes               */
  int    new;   /* index of master who receives new users          */

  bolist bo[BOUNCETAB];    /* bounced ports list                   */
} con;

void con_init(con *cp, int size, short port, int trblk, int ear);
void con_reset(con *cp);    /* Close conference and re-set socket  */
void con_clear(con *cp);    /* Close conference AND its ear socket */
void con_shutdown(con *cp, int e2);   /* Disconnect all users      */
void con_left(con *cp, user *up);     /* Add user to 'left' list   */

void con_run(con *cp, int min);       /* Loop handling user input  */
void con_deliver(con *cp, int who);   /* Deliver enqueue a message */
void con_command(con *cp, int who, int *flag);
void con_login(con *cp, int who);     /* Handle a new login        */
void con_change(con *cp, int who);    /* Handle a name change      */
void con_warnsd(con *cp, int when);   /* Handle a new warning      */
void con_newwarn(con *cp, int who);   /* Post a new warning        */

/* These functions don't actually do anything at the moment...     */
int  con_isbounced(con *cp, int ip, short port);
int  con_bounce(con *cp, int ip, short port);
void con_nbounce(con *cp, int ip, short port);
int  con_numbounced(con *cp);

void con_newuser(con *cp);   /* Accept a new connexion to the con  */
void con_exuser(con *cp, int who, int how);  /* Kill or disconnect */
void con_give_map(con *cp, int who, byte *map);  /* Transfer users */
void con_normalize(con *cp, int who);        /* Un-masterify       */

void con_notify(con *cp, int ex, char *msg);
void con_warn(con *cp, char *msg);
int  con_post(con *cp, mptr msg);
int  con_post_one(con *cp, mptr msg, int who);

/* These functions handle sending data out over connexions          */
void user_transmit(con *cp, int who);     /* send message to user   */
void user_sendblk(con *cp, int who);      /* keep sending it        */
void user_string(con *cp, int who, char *msg, int len);

#endif /* end XCAL_CON_H_ */
