/*
    lang.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Constant prompt strings for XCaliber Mark II
 */

#ifndef XCAL_LANG_H_
#define XCAL_LANG_H_

enum lang_tag {
  confull, conterm, welcome, entname, mwelc, intro, tlkwith,
  newuser, donemsg, msgfrom, allfrom, namenot, exnot, killnot,
  speak, outmsg, youout, igmsg, discmsg, norcpt, sentmsg, cmderr,
  fmterr, invarg, unimp, newwarn, newname, noleft, nomsgs, mnsmsg, 
  empty, badchar, noalls, linmsg, noport, rpmsg, bounced, blfull,
  nbport, nowmast, normal, passed, mptmmsg, pnumsg, timewrn, cantexp, 
  fnfmsg, leftmsg, lefthdr, upat, upat2, clkmsg, timelft, numusrs, 
  infomsg, xcalver, antemer, postmer, secsing, secplur, minsing, 
  minplur, newlang, nolang, curlang, crunow, coresize,
  helptop, aytmsg, brkmsg,  /* leave these last, please */
  num_lang_strs
};

#define MSG(M)    lang_tab[(M)]
#define XMSG(M)   lang_tab[(M)], strlen(lang_tab[(M)])

extern char  *lang_dir;
extern char **lang_tab;

char **lang_load(char *lng, int *ns);
void  lang_free(char **ltab, int ns);
void  lang_reset(void);
int   lang_install(char *lang);

#endif  /* end XCAL_LANG_H_ */
