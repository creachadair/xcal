/*
    default.h

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Default constant values for XCaliber Mark II
 */

#ifndef XCAL_DEFAULT_H_
#define XCAL_DEFAULT_H_

/*------------------------------------------------------------------------*/
/* 
   Default directories for help, language files, and so forth
 */

/* DEFAULT_BASEDIR - where to base all these other directories from.  
   This path should be absolute, and should end in a '/'                  */
#define DEFAULT_BASEDIR    "/usr/local/share/xcal/"

/* DEFAULT_HELP_DIR - where to look for the help text files               */
#define DEFAULT_HELP_DIR   DEFAULT_BASEDIR "help"

/* DEFAULT_LANG_DIR - where to look for compiled language files           */
#define DEFAULT_LANG_DIR   DEFAULT_BASEDIR "lang" 

/* DEFAULT_LOG_DIR - where to write the conference log file, if enabled   */
#define DEFAULT_LOG_DIR    DEFAULT_BASEDIR

/* DEFAULT_PRIV_DIR - where to look for the privilege database            */
#define DEFAULT_PRIV_DIR   DEFAULT_BASEDIR

/*------------------------------------------------------------------------*/
/* 
   Default strings and other constant values
 */

/* DEFAULT_LANG_EMPTY - message string to use when the current language
   doesn't define anything for a particular situation                     */
#define DEFAULT_LANG_EMPTY "<no string value>\r\n"

/* DEFAULT_TABSTR - tabs are expanded to this string on output            */
#define DEFAULT_TABSTR     "    "

/* DEFAULT_PATHLEN - if maximum path length is not given by the system
   header files, it defaults to this value, which determines the size
   of the buffer to allocate for a path name                              */
#define DEFAULT_PATHLEN    1024

/* DEFAULT_DBLDIG - if maximum float precision is not given by the system
   header files, it defaults to this value                                */
#define DEFAULT_DBLDIG     15

/* DEFAULT_MAXUSERS - the maximum number of users permitted to connect to
   a given conference.  This should not be increased unless you have a damn
   good idea what you're doing                                            */
#define DEFAULT_MAXUSERS   40

/* DEFAULT_LOG_ENABLED - determines whether logging is on by default.  If
   this is 1, logging is enabled; if it is 0, it is disabled              */
#define DEFAULT_LOG_ENABLED 0

/* DEFAULT_PORT - sets the default listener port                          */
#define DEFAULT_PORT       2456

#endif /* end XCAL_DEFAULT_H_ */
