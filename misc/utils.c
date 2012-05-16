/*
 * utils.c -- utility functions
 *
 * author:      QIU Shuang
 * modified on: May 9, 2012
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "utils.h"

#define WHITE_SPACE    " \n\r\t\f"

/* ---------------------------------------------------------------------------
  Name       : replace - Search & replace a substring by another one. 
  Creation   : QIU Shuang, May 10, 2012 based on Thierry Husson, Sept 2010
  Parameters :
      str    : Big string where we search
      oldstr : Substring we are looking for
      newstr : Substring we want to replace with
  Returns    : Pointer to original string.
--------------------------------------------------------------------------- */
char* replace(char *str, const char *oldstr, const char *newstr)
{
    int oldlen = strlen(oldstr);        /* Length of old pattern */
    int newlen = strlen(newstr);        /* Length of new pattern */
    char *ptr = str;
    const char *itr = str;
    const char *occur = NULL;           /* First occur of old pattern */

    while ((occur = strstr(itr, oldstr))) {
        int size = (int)(occur - itr);
        memcpy(ptr, itr, size);
        ptr = ptr + size;
        memcpy(ptr, newstr, newlen);
        itr = occur + oldlen;
        ptr = ptr + newlen;
    }
    while ((*ptr++ = *itr++));

    return str;
}

char *trim(char *str) {
    char       *itr  = str;
    const char *scan = str;
    int         len  = 0;

    while (scan && *scan && strchr(WHITE_SPACE, *scan)) {
        scan++;
    }   
    len = strlen(scan);
    strncpy(itr, scan, len);
    itr = itr + len - 1;
    while ((itr >= str) && strchr(WHITE_SPACE, *itr)) {
        itr--;
    }
    *(itr + 1) = '\0';                  /* End */

    return str;
}

int lsbPos(int val) {
    unsigned int v = (unsigned)val;
    int r;
    int MultiplyDeBruijnBitPosition[32] =
    { 
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
    
    r = MultiplyDeBruijnBitPosition[((unsigned)((v & -v) * 0x077CB531U))
        >> 27];
    
    return r;
}

char *unquote(char *str, char quote) {
    char       *buf_itr = str;
    const char *src_itr = str;
    char tmp = '\0';
    while ((tmp = *src_itr)) {
        if (tmp != quote) {
            *buf_itr++ = tmp;
        }
        src_itr++;
    }
    /* Clear rest buffer */
    while ((buf_itr != src_itr)) {
        *buf_itr++ = '\0';
    }

    return str;
}
