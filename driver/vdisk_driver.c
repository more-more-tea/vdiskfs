/*
 * vdisk_driver.c -- driver implementation for vdisk
 *
 * author:     QIU Shuang
 * created on: May 7, 2012
 */
#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include <json/json.h>

#include "hmac/hmac_sha2.h"
#include "misc/poster.h"
#include "misc/utils.h"

#include "vdisk_driver.h"

static char token[BUFFER_SIZE];         /* Access token */
static int  vdisk_errno;                /* Error code for vdisk */

static FILE *mapping;

// static FILE *output;

/* Callback functions for curl */
/* Get token */
/* Function invoked when send data to peer */
static size_t rd_callback(void *ptr, size_t size, size_t nmb,
        void *data) {
    struct request *request  = (struct request *)data;
    int             buf_len  = size * nmb;
    int             data_len = request->buf_left;

    int copy_len = (buf_len < data_len) ? buf_len : data_len;
    char *dst = (char *)ptr;
    strncpy(dst, request->buffer, copy_len);
    request->buffer   = request->buffer + copy_len;
    request->buf_left = request->buf_left - copy_len;
/*
if (output == NULL)
    output = fopen("driver/output", "wb+");
fprintf(output, "I've sent all.\n");
fflush(output);
*/

    return copy_len;
}

/* Function invoked when receive data from peer */
static size_t wt_callback(char *ptr, size_t size, size_t nmb,
        void *data) {
/*
if (!output) {
    output = fopen("driver/output", "wb+");
}
*/
    int len = size * nmb;

    struct response *rsp = (struct response *)data;
    int byte = len < rsp->buf_left ? len : rsp->buf_left;
/*
fprintf(output, "========================================\n");
fwrite(ptr, size, nmb, output);
fprintf(output, "\n");
fprintf(output, "----------------------------------------\n");
fwrite(rsp->buffer, rsp->buf_offset, 1, output);
fprintf(output, "that's currently what I get. And offset: %d, left: %d\n", rsp->buf_offset, rsp->buf_left);
fflush(output);
*/
    if (rsp->buf_offset == 0) {
        memset(rsp->buffer, 0, rsp->buf_left);
    }
    memcpy(rsp->buffer + rsp->buf_offset, ptr, byte);
    rsp->buf_offset += byte;
    rsp->buf_left   -= byte;
/*
fprintf(output, "Now I have: \n");
fwrite(rsp->buffer, rsp->buf_offset, 1, output);
fflush(output);
*/

    return byte;
}

/* Convert bit stream to hex */
static const char *base16lower(const char *stream, char *buf, int len) {
    static char map[] = {'0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    int i, j;
    for (i = 0, j = 0; i < len; i = i + BITS_HALF_BYTE, j++) {
        int index = i / BITS_PER_BYTE;
        unsigned char value = 0;
        if (i % BITS_PER_BYTE) {
            value = stream[index] & LOW_BYTE;
        } else {
            value = (stream[index] & HIGH_BYTE) >> BITS_HALF_BYTE;
        }

        buf[j] = map[value];
    }

    return buf;
}

/*
 * Map block number to file descriptor
 * if a proper value found, return 1;
 * else 0
 */
static int map_blk_to_fid(int blk_no, char *fid, int buf_len) {
    fseek(mapping, blk_no * VDISK_FID_LENGTH, SEEK_SET);
    if (feof(mapping)) {
        fprintf(stderr, "Cannot map block #%d to a proper remote file.\n",
            blk_no);

        return 0;
    } else {
        fseek(mapping, blk_no * VDISK_FID_LENGTH, SEEK_SET);
        fread(fid, buf_len, 1, mapping);

        return 1;
    }
}

/* Parse json string */
int parse_json_string(const char *json,
        int (*parser)(struct json_object *, char *, int),
        char *buffer, int buf_len) {
/*
fprintf(output, "parsing string: %s|\n", json);
fflush(output);
*/
    int return_code = VDISK_RETURN_OK;
    struct json_object *json_obj = json_tokener_parse(json);
    /* Get error code */
    struct json_object *json_err_code =
        json_object_object_get(json_obj, VDISK_JSON_ERRCODE);
    return_code = json_object_get_int(json_err_code);
printf("return code: %d\n", return_code);
    if (return_code == VDISK_REQUEST_OK) {
        if (parser) {
            return_code = parser(json_obj, buffer, buf_len);
        }
        /* else we just want to get the request code */
    } else if (return_code == VDISK_INVALID_TOKEN) {
        /* TODO all the parameter should be programmed dynamically */
        int a = authenticate(token, "qshhnkf@gmail.com", "qshhn2951622",
            "1907117890", "a9edcc374f4a2a852e1e1b79dc655a43", "local",
            BUFFER_SIZE);
    }
    json_object_put(json_err_code);
    json_object_put(json_obj);

    return return_code;
}

/* Get token from json string */
int get_token_from_json(struct json_object *json_obj, char *buffer,
        int buf_len) {
    struct json_object *json_obj_data =
        json_object_object_get(json_obj, VDISK_JSON_DATA);
    if (json_obj_data) {
        struct json_object *json_obj_token =
            json_object_object_get(json_obj_data, VDISK_JSON_TOKEN);
        if (json_obj_token) {
            strncpy(buffer, json_object_to_json_string(json_obj_token),
                buf_len);
            /* Remote heading and tailing quote of json value */
            unquote(buffer, VDISK_JSON_QUOTE);

            json_object_put(json_obj_token);
        } else {
            vdisk_errno = VDISK_GTOK_FAIL;
            fprintf(stderr,
                "Authenticate OK, but cannot get access token.\n");
        }
    } else {
            vdisk_errno = VDISK_NON_DATA;
    }
    json_object_put(json_obj_data);

    return vdisk_errno;
}

/* Get file id from json */
int get_fid_from_json(struct json_object *json_obj, char *fid, int length) {
// printf("json: %s\n", json_object_json_string(json_obj));
printf("hello\n");
    struct json_object *json_obj_data   =
        json_object_object_get(json_obj, VDISK_JSON_DATA);
    if (json_obj_data) {
        struct json_object *json_obj_fid =
            json_object_object_get(json_obj_data, VDISK_JSON_FID);
        strncpy(fid, json_object_to_json_string(json_obj_fid), length);
        unquote(fid, VDISK_JSON_QUOTE);
printf("fid: %s\n", fid);
        /* Release branch dependent object */
        json_object_put(json_obj_fid);
    } else {
        vdisk_errno = VDISK_NON_DATA;
    }
    json_object_put(json_obj_data);

    return vdisk_errno;
}

/* Get file download url from json */
static int get_url_from_json(struct json_object *json_obj, char *url,
        int buf_len) {
    struct json_object *json_obj_data =
        json_object_object_get(json_obj, VDISK_JSON_DATA);
    if (json_obj_data) {
        struct json_object *json_obj_url =
            json_object_object_get(json_obj_data, VDISK_JSON_URL);
        strncpy(url, json_object_to_json_string(json_obj_url), buf_len);
        /* Extract string contents */
        unquote(url, VDISK_JSON_QUOTE);
        /* Replace escaped characters */
        replace(url, "\\/", "/");
    } else {
        vdisk_errno = VDISK_NON_DATA;
    }

    return vdisk_errno;
}

/* Request for keep alive per 10 min */
static void keep_alive(int signo) {
    int return_code = VDISK_RETURN_OK;
    char url[BUFFER_SIZE];              /* URL to send the request */
    char postdata[BUFFER_SIZE];         /* Buffer for post data */
    char response[BUFFER_SIZE];         /* Buffer for response */
    memset(url, 0, BUFFER_SIZE);
    memset(postdata, 0, BUFFER_SIZE);
    memset(response, 0, BUFFER_SIZE);

    /* Compose url */
    strncpy(url, VDISK_BASE_URL, BUFFER_SIZE);
    strncat(url, VDISK_KTOK_RST, BUFFER_SIZE);

    /* Compose post data */
    sprintf(postdata, VDISK_KEEP_TOKEN, token);

    /* Request */
    struct request rst;
    rst.buffer   = postdata;
    rst.buf_left = strlen(postdata);
    /* Response */
    struct response rsp;
    rsp.buffer   = response;
    rsp.buf_left = BUFFER_SIZE;
    rsp.buf_offset = 0;

    /* Here we go */
    post(url, &rst, &rsp, rd_callback, wt_callback);
    return_code = parse_json_string(rsp.buffer, NULL, NULL, 0);
    if (return_code != VDISK_REQUEST_OK) {
        alarm(VDISK_CANCEL_ALARM);
    } else {
        alarm(VDISK_KTOK_INTERVAL);
    }
}

int authenticate(char *token_buf, const char *account, const char *passwd,
        const char *appkey, const char *appsecret, const char *apptype,
        int buflen) {
    /* No mapping file found, open/create a new file */
    if (mapping == NULL) {
        if (access(VDISK_MAPPING_PATH, F_OK) != -1) {
            mapping = fopen(VDISK_MAPPING_PATH, "rb+");
        } else {
            mapping = fopen(VDISK_MAPPING_PATH, "wb+");
        }
        if (mapping == NULL) {
            fprintf(stderr, "Cannot open mapping file. Program exits.\n");

            exit(VDISK_MAPPING_FAIL);
        }
    }

printf("here?\n");
    int return_code = VDISK_RETURN_OK;

    char rst_buffer[BUFFER_SIZE];       /* Buffer for post data */
    char rsp_buffer[BUFFER_SIZE];       /* Buffer for response data */
    char umac[BUFFER_SIZE];             /* Buffer for bitstream mac & url */
    char signature[BUFFER_SIZE];        /* Buffer for signature */
    char auth[BUFFER_SIZE];             /* Buffer for auth info */
    memset(rst_buffer, 0, BUFFER_SIZE);
    memset(rsp_buffer, 0, BUFFER_SIZE);
    memset(umac, 0, BUFFER_SIZE);
    memset(signature, 0, BUFFER_SIZE);
    memset(auth, 0, BUFFER_SIZE);

    time_t t = time(NULL);
    /* Compose request */
    sprintf(rst_buffer, VDISK_GTOK_USINED, account, appkey, passwd, t);
    /* Calculate SHA-256 Message Authentication Code */
    hmac_sha256((const unsigned char *)appsecret, strlen(appsecret),
        (const unsigned char *)rst_buffer, strlen(rst_buffer),
        (unsigned char *)umac, BUFFER_SIZE);
    /* Convert bit represented mac into hex string */
    base16lower(umac, signature, VDISK_MAC_LENGTH);

    sprintf(auth, VDISK_GTOK_SIGNED, signature, apptype);
    strncat(rst_buffer, auth, BUFFER_SIZE);

    /* Post data */
    memset(umac, 0, BUFFER_SIZE);
    strncpy(umac, VDISK_BASE_URL, BUFFER_SIZE);
    strncat(umac, VDISK_GTOK_RST, BUFFER_SIZE);

    struct request  rst;
    rst.buffer = rst_buffer;
    rst.buf_left = strlen(rst_buffer);

    struct response rsp;
    rsp.buffer   = rsp_buffer;
    rsp.buf_left = BUFFER_SIZE;
    rsp.buf_offset = 0;
printf("request token: %s\n", rst_buffer);
    /* Post request */
    post(umac, &rst, &rsp, rd_callback, wt_callback);
printf("I find the bug.\n");

    /* Try to get access token */
    return_code = parse_json_string(rsp.buffer, get_token_from_json,
        token_buf, BUFFER_SIZE);

    /* Install keep_alive timer */
    if (signal(SIGALRM, keep_alive) == SIG_ERR) {
        fprintf(stderr, "Cannot install keep_alive handler.\n");
    } else {
        alarm(VDISK_KTOK_INTERVAL);
    }

    return return_code;
}

/* Retrieve block from remote storage */
int get_block(u32 blk_no, int blk_size, char *buf, int buf_len) {
    int return_code = VDISK_RETURN_OK;
    if (blk_size > buf_len) {
        return_code = VDISK_BUF_OVERFLOW;
    } else {
        char url[BUFFER_SIZE];          /* URL for the request */
        char postdata[BUFFER_SIZE];     /* Post data */
        char response_buf[BUFFER_SIZE]; /* Response buffer */
        char fid[VDISK_FID_LENGTH];     /* Get mapping fid from blk_no */
        memset(url, 0, BUFFER_SIZE);
        memset(postdata, 0, BUFFER_SIZE);
        memset(response_buf, 0, BUFFER_SIZE);
        memset(fid, 0, VDISK_FID_LENGTH);

        /* To retrieve a block from server, first get the download URL */
        strncpy(url, VDISK_BASE_URL, BUFFER_SIZE);
        strncat(url, VDISK_INFO_RST, BUFFER_SIZE);
        map_blk_to_fid(blk_no, fid, VDISK_FID_LENGTH);
        /* Compose post data */
        sprintf(postdata, VDISK_GINFO, token, fid);

        /* Request and response buffer */
        struct request rst;
        rst.buffer   = postdata;
        rst.buf_left = strlen(postdata);

        struct response rsp;
        rsp.buffer   = response_buf;
        rsp.buf_left = BUFFER_SIZE;
        rsp.buf_offset = 0;
        /* Fire! */
        return_code = post(url, &rst, &rsp, rd_callback, wt_callback);
/*
fprintf(output, "I'll post request.\n");
fflush(output);
*/
        int retry = VDISK_RETRY_ON_FAIL;
/*
fprintf(output, "I'll post request.\n");
fprintf(output, "And I get the data: %s\n", rsp.buffer);
fflush(output);
*/
        /* Extract download link */
        do {
/*
fprintf(output, "I'll post request.\n");
fflush(output);
*/
            rsp.buf_left   = BUFFER_SIZE;
            rsp.buf_offset = 0;

            retry = retry - 1;
/*
fprintf(output, "I'll post request.\n");
fprintf(output, "json json: %s\n", rsp.buffer);
fflush(output);
*/
            return_code = parse_json_string(rsp.buffer, get_url_from_json,
                url, BUFFER_SIZE);
/*
fprintf(output, "I get something.\n");
fflush(output);
*/
        } while ((return_code != VDISK_REQUEST_OK) && retry);
/*
fprintf(output, "How could I be here?");
fflush(output);
*/

        if (return_code == VDISK_REQUEST_OK) {
            rsp.buffer   = buf;
            rsp.buf_left = buf_len;
            rsp.buf_offset = 0;
            return_code = get(url, &rsp, wt_callback);
        }
    }

    return return_code;
}

/* Put block onto remote storage */
int put_block(u32 blk_no, int blk_size, char *buf, int buf_len, char *fid,
        int fid_len) {
printf("here in put_block, #%d\n", blk_no);
    int return_code = VDISK_RETURN_OK;
    if (blk_size > buf_len) {
        return_code = VDISK_BUF_OVERFLOW;
    } else {
        char url[BUFFER_SIZE];          /* URL to upload file */
        char buffer[BUFFER_SIZE];       /* For server side response */
        char blk_id[BUFFER_SIZE];       /* Data to post to server */
        memset(url, 0, BUFFER_SIZE);
        memset(buffer, 0, BUFFER_SIZE);
        memset(blk_id, 0, BUFFER_SIZE);

        /* Compose URL */
        strncpy(url, VDISK_BASE_URL, BUFFER_SIZE);
        strncat(url, VDISK_ULDF_RST, BUFFER_SIZE);

        /* Block ID or remote filename */
        sprintf(blk_id, "%d", blk_no);

        struct response rsp;            /* Get cloud response */
        rsp.buffer   = buffer;

        int retry = VDISK_RETRY_ON_FAIL;
        do {
           /* Reset response */
           rsp.buf_left = BUFFER_SIZE;
           rsp.buf_offset = 0;

           retry = retry - 1;
            return_code =
                multipart_post(url, blk_id, buf, buf_len, &rsp, wt_callback,
                3,
                VDISK_NAME_TOKEN, token,
                VDISK_NAME_OVERWRITE, VDISK_CONT_YES,
                VDISK_NAME_DIRID, VDISK_CONT_ROOTDIR);

            return_code = parse_json_string(rsp.buffer, get_fid_from_json,
                fid, fid_len);
        } while ((return_code != VDISK_REQUEST_OK) && retry);
        /* Record fid */
        if (return_code == VDISK_REQUEST_OK) {
            fseek(mapping, blk_no * VDISK_FID_LENGTH, SEEK_SET);
            fwrite(fid, VDISK_FID_LENGTH, sizeof(char), mapping);
        }
   }

    return return_code;
}

/*
int main(int argc, char **argv) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    authenticate(token, "qshhnkf@gmail.com", "qshhn2951622",
        "1907117890", "a9edcc374f4a2a852e1e1b79dc655a43", "local", BUFFER_SIZE);
printf("auth DONE.\n");
printf("token: %s\n", token);
printf("buffer: %s\n", buffer);
    buffer[0] = '1';
    buffer[1] = '2';
    buffer[2] = '3';

    // put_block(1, 1 << 5, token, buffer, 1 << 5, buffer, BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);
    char buf[1024];
    get_block(1, 1024, buf, 1024);

char tmp[2048];
printf("dump: %s\n", base16lower(buf, tmp, 2048));

    return 0;
}
*/
