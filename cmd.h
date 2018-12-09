/*
    cmd.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Command parser and handlers for XCaliber Mark II
 */

#ifndef XCAL_CMD_H_
#define XCAL_CMD_H_

#include "con.h"

/* Bitmap fiddling macros                                              */
#define  SETMAP(M, B)   do{(M)[(B)/CHAR_BIT]|=(1<<((B)%CHAR_BIT));} while(0)
#define  MAPBIT(M, B)   ((M)[(B)/CHAR_BIT]&(1<<((B)%CHAR_BIT)))
#define  MAPSIZE(C)     (((C)->size/CHAR_BIT)+(((C)->size%CHAR_BIT)?1:0))

/*
  Compare two strings case-insensitively, permitting wildcard stops
  in the second (cmd) parameter
 */
int cmd_comp(char *str, char *cmd);

/* Format flags for cmd_who_entry()                                  */
#define  FMT_PORT     1     /* include port description, possibly RP */
#define  FMT_NAME     2     /* include name (ident) field            */
#define  FMT_STAT     4     /* include status and state information  */
#define  FMT_LORD     8     /* override RP and include IP address    */
#define  FMT_ALL      16    /* display all users, in all subcons     */
#define  FMT_SAME     32    /* display only users at same level      */
#define  FMT_BELOW    64    /* display only users below you (or you) */
#define  FMT_SUBCON   96    /* display only users in your subcon     */

void cmd_who_entry(con *cp, int who, int fbo, stream *sp, int format);

/* A plethora of command handlers... */

void cmd_tell(con *cp, int who, int *flag, char *rest);
void cmd_who_raw(con *cp, int who, int *flag, char *rest);
void cmd_who(con *cp, int who, int *flag, char *rest);
void cmd_bye(con *cp, int who, int *flag, char *rest);
void cmd_port(con *cp, int who, int *flag, char *rest);
void cmd_help(con *cp, int who, int *flag, char *rest);
void cmd_exp(con *cp, int who, int *flag, char *rest);
void cmd_info(con *cp, int who, int *flag, char *rest);
void cmd_vers(con *cp, int who, int *flag, char *rest);
void cmd_id(con *cp, int who, int *flag, char *rest);
void cmd_im(con *cp, int who, int *flag, char *rest);
void cmd_ra(con *cp, int who, int *flag, char *rest);
void cmd_aa(con *cp, int who, int *flag, char *rest);
void cmd_ig(con *cp, int who, int *flag, char *rest);
void cmd_ac(con *cp, int who, int *flag, char *rest);
void cmd_an(con *cp, int who, int *flag, char *rest);
void cmd_rn(con *cp, int who, int *flag, char *rest);
void cmd_rp(con *cp, int who, int *flag, char *rest);
void cmd_rc(con *cp, int who, int *flag, char *rest);
void cmd_oc(con *cp, int who, int *flag, char *rest);
void cmd_out(con *cp, int who, int *flag, char *rest);
void cmd_lin(con *cp, int who, int *flag, char *rest);
void cmd_time(con *cp, int who, int *flag, char *rest);
void cmd_left(con *cp, int who, int *flag, char *rest);
void cmd_rb(con *cp, int who, int *flag, char *rest);
void cmd_ab(con *cp, int who, int *flag, char *rest);
void cmd_dw(con *cp, int who, int *flag, char *rest);
void cmd_clock(con *cp, int who, int *flag, char *rest);
void cmd_usrs(con *cp, int who, int *flag, char *rest);
void cmd_all(con *cp, int who, int *flag, char *rest);
void cmd_tty(con *cp, int who, int *flag, char *rest);
void cmd_ru(con *cp, int who, int *flag, char *rest);
void cmd_warn(con *cp, int who, int *flag, char *rest);
void cmd_kill(con *cp, int who, int *flag, char *rest);
void cmd_en(con *cp, int who, int *flag, char *rest);
void cmd_dis(con *cp, int who, int *flag, char *rest);
void cmd_xwho(con *cp, int who, int *flag, char *rest);
void cmd_below(con *cp, int who, int *flag, char *rest);
void cmd_mty(con *cp, int who, int *flag, char *rest);
void cmd_norm(con *cp, int who, int *flag, char *rest);
void cmd_give(con *cp, int who, int *flag, char *rest);
void cmd_xdl(con *cp, int who, int *flag, char *rest);
void cmd_xlang(con *cp, int who, int *flag, char *rest);
void cmd_xtend(con *cp, int who, int *flag, char *rest);
void cmd_xyzzy(con *cp, int who, int *flag, char *rest);
void cmd_xreset(con *cp, int who, int *flag, char *rest);
void cmd_nimp(con *cp, int who, int *flag, char *rest);

#endif /* end XCAL_CMD_H_ */
