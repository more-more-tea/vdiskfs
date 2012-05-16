/*
 * poster.c -- implementation of an interface for local and remote com
 *
 * author:      QIU Shuang
 * modified on: May 8, 2012
 */
#include <string.h>
#include <curl/curl.h>

#include <stdio.h>

#include "poster.h"

static FILE *output;

/* Simple get request */
int get(const char *url,
        struct response *response,
        size_t (*wt_callback)(char *, size_t, size_t, void *)) {
    CURLcode return_code = CURLE_OK;
    CURL    *curl        = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wt_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        return_code = curl_easy_perform(curl);

        /* Clean the scene */
        curl_easy_cleanup(curl);
    }

    return return_code;
}

/* Simple post request */
int post(const char *url,
        struct request *request, struct response *response,
        size_t (*rd_callback)(void *, size_t, size_t, void *),
        size_t (*wt_callback)(char *, size_t, size_t, void *)) {
if (output == NULL) {
    output = fopen("misc/output", "wb+");
}
fprintf(output, "I should post: %s to %s\n", request->buffer, url);
fflush(output);
    CURL *curl;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url); /* Set url to request */
        curl_easy_setopt(curl, CURLOPT_POST, 1L); /* Post not put */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, rd_callback);
                                        /* How data is post */
        curl_easy_setopt(curl, CURLOPT_READDATA, request);
                                        /* Data to be post */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request->buf_left);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wt_callback);
                                        /* How data is received */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
                                        /* Where to store incoming data */
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        curl_easy_perform(curl);
fprintf(output, "hey, I post it: %s\n", request->buffer);
fflush(output);
        curl_easy_cleanup(curl);
    }

    return 0;
}

/* Multi-part form post */
int multipart_post(const char *url,
        char *buf_id, char *data, int data_len,
        struct response *response,
        size_t (*wt_callback)(char *, size_t, size_t, void *),
        int argc, ...) {
    CURL                 *curl       = NULL;
    struct curl_httppost *formpost   = NULL;
    struct curl_httppost *lastpost   = NULL;

    /* Fill in the file upload field */
    va_list argv;
    va_start(argv, argc);
    int i;
    for (i = 0; i < argc; i++) {
        char *name    = va_arg(argv, char *);
        char *content = va_arg(argv, char *);
        curl_formadd(&formpost, &lastpost,
            CURLFORM_COPYNAME, name,
            CURLFORM_COPYCONTENTS, content,
            CURLFORM_END);
    }
    va_end(argv);
    curl_formadd(&formpost, &lastpost,
        CURLFORM_COPYNAME, "send",
        CURLFORM_BUFFER, buf_id,
        CURLFORM_BUFFERPTR, data,
        CURLFORM_BUFFERLENGTH, data_len,
        CURLFORM_END);

    /* Post form data */
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);           /* URL */
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost); /* Form data */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wt_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* Here we go */
    curl_easy_perform(curl);

    /* Clean the scene */
    curl_easy_cleanup(curl);
    curl_formfree(formpost);

    return 0;
}
