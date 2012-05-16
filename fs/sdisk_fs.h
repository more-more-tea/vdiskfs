/*
 * sdisk_fs.h -- this file defines meta-data for the running file system
 *
 * author:      QIU Shuang
 * modified on: May 7, 2012
 */
#ifndef SDISK_FS_H
#define SDISK_FS_H

#include "misc/types.h"

#define SDISK_NDIR_BLOCKS              6
#define SDISK_IND_BLOCK                SDISK_NDIR_BLOCKS
#define SDISK_DIND_BLOCK               (SDISK_IND_BLOCK + 1)
#define SDISK_TIND_BLOCK               (SDISK_DIND_BLOCK + 1)
#define SDISK_N_BLOCKS                 (SDISK_TIND_BLOCK + 1)

/*
 * Structure of a superblock on the file system
 */
struct sdisk_super_block {
    u32      s_inodes_count;           /* Inodes count */
    u32      s_blocks_count;           /* Blocks count */
    u32      s_free_blocks_count;      /* Free blocks count */
    u32      s_free_inodes_count;      /* Free inodes count */
    u32      s_free_block;             /* Next free data block number */
    u32      s_free_inode;             /* Next free inode number */
    u32      s_log_block_size;         /* Block size */
    u32      s_mtime;                  /* Mount time */
    u32      s_wtime;                  /* Write time */
    u16      s_mount_count;            /* Mount count */
    u16      s_max_mount_count;        /* Maximal mount count */
    u16      s_magic;                  /* Magic number */
    u16      s_state;                  /* File system state */
    u16      s_errors;                 /* Behaior when detecting errors */
};

/*
 * Structure of an inode on the disk
 * Compatible with inode structure in MINIX
 */
struct sdisk_inode {
    u16      i_mode;                   /* File mode */
    u16      i_links_count;            /* Links count */
    u16      i_uid;                    /* Set aside for extension */
    u16      i_gid;                    /* Set aside for extension */
    u32      i_inode_no;               /* Inode number of current inode */
    u32      i_size;                   /* File size */
    u32      i_atime;                  /* Access time */
    u32      i_ctime;                  /* Creation time */
    u32      i_mtime;                  /* Modification time */
    u32      i_block[SDISK_N_BLOCKS];  /* Pointers to blocks */
};

#define SDISK_NAME_LEN                 60 /* For padding */

/*
 * Structure of a directory entry
 */
struct sdisk_dir_entry {
    u32      inode;                    /* Inode number */
    char     name[SDISK_NAME_LEN];     /* File name */
};

#endif
