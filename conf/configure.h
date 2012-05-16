/*
 * configure.h -- configurations and settings
 *
 * author:      QIU Shuang
 * modified on: May 7, 2012
 */
#ifndef SDISK_CONF_H
#define SDISK_CONF_H

/* system general configuration */
enum SysKey{
    SYS_DISKS     = 0,     /* clouds in use */
    SYS_BLOCKSIZE = 1,     /* remote block size */
    SYS_ALL
};

/* available cloud storages */
enum Disk{
    AVAIL_VDISK = 0,      /* vdisk */
    AVAIL_OSS   = 1,      /* open storage service from aliyun */
    AVAIL_SSS   = 2,      /* storage service from amazon */
    AVAIL_ALL
};

/* vdisk specific configuration */
enum VdiskKey{
    VDISK_ACCOUNT = 0,    /* vdisk account */
    VDISK_PASSWD  = 1,    /* vdisk password */
    VDISK_APP_KEY = 2,    /* vdisk app key */
    VDISK_APPSECRET   = 3,/* vdisk app secret key */
    VDISK_ACCOUNTTYPE = 4,/* vdisk account type */
    VDISK_ALL
};

#endif
