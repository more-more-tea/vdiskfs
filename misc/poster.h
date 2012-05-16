/*
 * poster.h -- an interface for app to communicate with remote storage
 *
 * author:      QIU Shuang
 * modified on: May 8, 2012
 */
#ifndef SDISK_MISC_POSTER_H
#define SDISK_MISC_POSTER_H

#include <stdarg.h>

struct request {
    char *buffer;
    int buf_left;
};

struct response {
    char *buffer;
    int  buf_left;
    int  buf_offset;
};

/* Get data */
int get(const char *url,
    struct response *response,
    size_t (*wt_callback)(char *, size_t, size_t, void *));

/* Post data */
int post(const char *url,
    struct request *request, struct response *response,
    size_t (*rd_callback)(void *, size_t, size_t, void *),
    size_t (*wt_callback)(char *, size_t, size_t, void *));

/* Multi-part post data */
int multipart_post(const char *url,
    char *buf_id, char *data, int data_len, struct response *response,
    size_t (*wt_callback)(char *, size_t, size_t, void *),
    int argc, ...);

#endif
