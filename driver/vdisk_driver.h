/*
 * vdisk_driver.h -- driver to vdisk
 *
 * author:      QIU Shuang
 * modified on: May 7, 2012
 */
#ifndef VDISK_DRIVER_H
#define VDISK_DRIVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "misc/constants.h"
#include "misc/types.h"

#define VDISK_MAC_LENGTH         256    /* Mac length, 256-bit mac */
#define VDISK_CANCEL_ALARM       0      /* Cancel alarm */
                                        /* Send keep alive request/10min */
#define VDISK_KTOK_INTERVAL      (10 * 60)
#define VDISK_FID_LENGTH         16

#define VDISK_MAPPING_PATH       "configure/vdisk.map"

/* String literals */
#define VDISK_BASE_URL     "http://openapi.vdisk.me/?"  /* Base url */
/* All requests */
/* Authenticate and Synchronize */
#define VDISK_SYNC_RST     "a=keep"                     /* Keep sync */
#define VDISK_GTOK_RST     "m=auth&a=get_token"         /* Get token */
#define VDISK_KTOK_RST     "m=user&a=keep_token"        /* Keep token */
/* FILE related requests */
#define VDISK_ULDF_RST     "m=file&a=upload_file"       /* Upload file */
#define VDISK_ULDS_RST     "m=file&a=upload_with_sha1"  /* Upload SHA1 */
#define VDISK_UFAS_RST     "m=file&a=upload_share_file" /* Upload & share */
#define VDISK_CPYF_RST     "m=file&a=copy_file"         /* Copy file */
#define VDISK_MOVF_RST     "m=file&a=move_file"         /* Move file */
#define VDISK_RNMF_RST     "m=file&a=rename_file"       /* Rename file */
#define VDISK_DELF_RST     "m=file&a=delete_file"       /* Delete file */
#define VDISK_QUTA_RST     "m=file&a=get_quota"         /* Quota info */
#define VDISK_INFO_RST     "m=file&a=get_file_info"
/* File sharing */
#define VDISK_SHAF_RST     "m=file&a=share_file"
#define VDISK_CSHA_RST     "m=file&a=cancel_share_file"

/*
 * Get token
 * parameterized by:
 * 1. account
 * 2. appkey
 * 3. password
 * 4. timestamp
 */
#define VDISK_GTOK_USINED  "account=%s&appkey=%s&password=%s&time=%ld"
#define VDISK_GTOK_SIGNED  "&signature=%s&app_type=%s"
#define VDISK_KEEP_TOKEN   "token=%s"

/* Parameterized by token and file identifier */
#define VDISK_GINFO        "token=%s&fid=%s"

/* Post block to root directory, overwrite if exists */
#define VDISK_NAME_TOKEN       "token"
#define VDISK_NAME_OVERWRITE   "cover"
#define VDISK_NAME_DIRID       "dir_id"
#define VDISK_NAME_DOLOGDIR    "dologdir"

#define VDISK_CONT_YES         "yes"
#define VDISK_CONT_NO          "no"
#define VDISK_CONT_ROOTDIR     "0"

/* Response string literals */
#define VDISK_JSON_QUOTE   '"'
#define VDISK_JSON_ERRCODE "err_code"
#define VDISK_JSON_DATA    "data"
#define VDISK_JSON_TOKEN   "token"
#define VDISK_JSON_FID     "fid"
#define VDISK_JSON_URL     "s3_url"

#define VDISK_RETRY_ON_FAIL 3

/* Status code */
#define VDISK_REQUEST_OK    0
#define VDISK_INVALID_TOKEN 702

/* Return code */
#define VDISK_RETURN_OK    0
#define VDISK_MAPPING_FAIL 1
#define VDISK_NON_DATA     252
#define VDISK_BUF_OVERFLOW 253
#define VDISK_AUTH_FAIL    254
#define VDISK_GTOK_FAIL    255

/* Get authentication to remote storage */
int authenticate(char *token, const char *account, const char *passwd,
        const char *appkey, const char *appsecret, const char *apptype,
        int buflen);

/* Retrieve block from remote storage */
int get_block(u32 blk_no, int blk_size, char *buf, int buf_len);

/* Put block onto remote storage */
int put_block(u32 blk_no, int blk_size, char *buf, int buf_len, char *fid, int fid_len);

#endif
