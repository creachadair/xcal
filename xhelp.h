/*
    xhelp.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Help system for XCaliber Mark II
 */

#ifndef XCAL_HELP_H_
#define XCAL_HELP_H_

#include <stdio.h>

#define HELP_FMT  -3    /* error -- format of table file is bad         */
#define HELP_TFNF -2    /* error -- toc file not found or unreadable    */
#define HELP_HFNF -1    /* error -- help file not found or unreadable   */
#define HELP_NF    0    /* topic was not found in help databaes         */
#define HELP_OK    1    /* okay -- found what you asked for             */

#define TOCFILE    "exptoc.x"  /* name of the compiled toc file         */

extern char *help_dir;  /* where to look for help files                 */

/*
  Look up a topic in the help database.  You should pass in the 
  name of the table file in 'toc'.  The return value is one of the
  values described above.  If HELP_OK is returned, then fp is 
  given a pointer to an open file stream containing the help data
  for the given topic.  Otherwise it is left alone.
 */
  
int help_lookup(char *toc, char *topic, FILE **fp);

#endif /* end XCAL_HELP_H_ */
