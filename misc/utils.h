/*
 * utils.h -- utility function
 *
 * author:      QIU Shuang
 * modified on: May 9, 2012
 */
#ifndef SDISK_MISC_UTIL_H
#define SDISK_MISC_UTIL_H

/* ---------------------------------------------------------------------------
  Name       : replace - Search & replace a substring by another one. 
  Creation   : QIU Shuang, May 10, 2012 based on Thierry Husson, Sept 2010
  Parameters :
      str    : Big string where we search
      oldstr : Substring we are looking for
      newstr : Substring we want to replace with
  Returns    : Pointer to the original string.
--------------------------------------------------------------------------- */
char* replace(char *str, const char *oldstr, const char *newstr);

/* Trim heading and tailing white space, etc. */
char *trim(char *str);

/* Remove heading and tailing quote's */
char *unquote(char *str, char quote);

/* Calculate the least significiant bit set */
int lsbPos(int val);

#endif
