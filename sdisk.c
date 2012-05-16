/*
 * sdisk.c -- entrance for the secure disk client
 *
 * author:      QIU Shuang
 * modified on: May 7, 2012
 */
#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fuse.h>
#include <unistd.h>
#include <errno.h>

#include "fs/sdisk_fs.h"
#include "conf/configure.h"
#include "driver/vdisk_driver.h"
#include "misc/constants.h"
#include "misc/utils.h"

#define othermode(r, w, x) ((r << 2) | (w << 1) | (x))
#define groupmode(r, w, x) (othermode(r, w, x) << 3)
#define ownermode(r, w, x) (othermode(r, w, x) << 6)
#define extramode(u, g, s) (othermode(u, g, s) << 9)

#define blocknumber(offset) ((offset) >> superblock.s_log_block_size)
#define blockoffset(offset) ((offset) & ~(1 << superblock.s_log_block_size))

#define CLOUD_ID_LEN       32           /* Length of cloud id */
#define VALUE_BUFFER_SIZE  128          /* Buffer size for key-value pair */
#define SUPERBLOCK_SIZE    512          /* Size of superblock */

#define GLOBAL_SETTINGS    2

/* Error code */
#define SDISK_FILE_INODE_ERR      253  /* Cannot load inodes */
#define SDISK_FILE_SUPERBLOCK_ERR 254  /* Cannot load superblock */
#define SDISK_FILE_OPEN_ERR       255  /* -1 for file open error */

/* String literals */
#define COMMENT        '#'
#define WHITE_SPACE    " \n\r\t\f"

#define FS_SEPERATOR   "/"              /* Platform-dependent seperator */
#define CONFIG_BASE    "configure"      /* Configuration directory */
#define SYSTEM_CONFIG  "sdisk.conf"     /* System-wise configuration */
#define CONFIG_EXP     ".conf"          /* Extension for configure file */
#define META           ".meta"          /* Meta-data for the file system */

#define ROOT_INODE_NR  1                /* Inode number start at 1 */
#define ROOT_BLOCK_NR  1                /* Block number start at 1 also */
#define FS_SELF        "."              /* Self directory */
#define FS_PARENT      ".."             /* Parent directory */





FILE *output;




/* Global parameter */
int    clouds = 0;                      /* # of backups, or # of clouds */
char  *diskID[AVAIL_ALL] = {"vdisk", "oss", "sss"};
char **cloudConfs[AVAIL_ALL];

char  *globalSettings[GLOBAL_SETTINGS]; /* Global settings */
char  *block;                           /* Block buffer */
int    block_size;                      /* Block size */

FILE                     *meta;         /* Metadata */
struct sdisk_super_block  superblock;   /* Superblock */
struct sdisk_inode        root;         /* Root inode */

/* TODO */
/* Get next free inode number */
static int next_inode_no() {
    return superblock.s_free_inode + 1;
}

/* TODO */
/* Get next free data block number */
static int next_data_block_no() {
    return superblock.s_free_block + 1;
}

/* TODO */
/* Get specified inode */
static int get_inode(struct sdisk_inode *inode, int inode_no) {
    int return_code = 0;
    if (fseek(meta, SUPERBLOCK_SIZE + sizeof(struct sdisk_inode)*inode_no,
            SEEK_SET) == -1) {
        return_code = -errno;
    } else if (fread((void *)inode, sizeof(struct sdisk_inode), 1, meta) < 1) {
        return_code = -errno;
    }

    return return_code;
}

/* TODO */
/* Need extended to handle doubly indirect reference */
static u32 get_data_blk_idx(const struct sdisk_inode *inode, int offset) {
    int blk_idx = blocknumber(offset);

    return inode->i_block[blk_idx];
}

/* TODO */
/* Update specified inode */
static int update_inode(struct sdisk_inode *inode ) {
    int return_code = 0;
    fseek(meta, SUPERBLOCK_SIZE + sizeof(struct sdisk_inode)*(inode->i_inode_no),
            SEEK_SET);
    if (fwrite((void *)inode, sizeof(struct sdisk_inode), 1, meta)
            < 1) {
        return_code = -errno;
    }

    return return_code;
}

/* TODO */
/* Update superblock */
static int update_superblock() {
    int return_code = 0;
    if (fseek(meta, 0, SEEK_SET) == -1) {
        return_code = -errno;
    } else if (fwrite((void *)&superblock, 1,
            sizeof(struct sdisk_super_block), meta)
            < sizeof(struct sdisk_super_block)) {
        return_code = -errno;
    }

    return return_code;
}

/* TODO */
/* Find offset of an empty slot in directory, -1 if full */
static int empty_dir_slot(struct sdisk_inode *inode, int *block_no,
        char *data, int len) {
    struct sdisk_dir_entry dir;
    int empty_slot = 0;
    int found  = 0;
    int offset = 0;

    int inode_blk_idx = 0;
    /* Direct reference */
    while (!found && (inode->i_block[inode_blk_idx])) {
        offset = 0;
        get_block(inode->i_block[inode_blk_idx], block_size, data, len);

        while (!found && (offset < len)) {
            memcpy((void *)&dir, data + offset, sizeof(struct sdisk_dir_entry));
fprintf(output, "entry inode: %d\tentry name: %s\n", dir.inode, dir.name);
fflush(output);
            if (!dir.inode) {
                found      = 1;
                *block_no  = inode->i_block[inode_blk_idx];
                empty_slot = offset;
            }
            offset = offset + sizeof(struct sdisk_dir_entry);
        }

        inode_blk_idx++;
    }

    /* Double-ly indirect reference & tripe-ly indirect reference */

    if (!found) {
        empty_slot = -1;
    }

    return empty_slot;
}

/* Create new inode */
static int new_inode(struct sdisk_inode *inode, int inode_no, int mode,
        int links_count, int size) {
    int return_code = 0;
    memset(inode, 0, sizeof(struct sdisk_inode));
    inode->i_mode        = mode;
    inode->i_links_count = links_count;
    inode->i_uid         = getuid();
    inode->i_gid         = getgid();
    inode->i_inode_no    = inode_no;
    inode->i_size        = size;
    inode->i_atime       = time(NULL);
    inode->i_ctime       = time(NULL);
    inode->i_mtime       = time(NULL);

    return_code = update_inode(inode);

    superblock.s_free_inode = next_inode_no();
    return_code = update_superblock();

    return return_code;
}

/* Create empty directory data block */
static int new_empty_dir(struct sdisk_inode *inode, int blk_no, int inode_no,
        int parent_inode_no) {
    int return_code = 0;
    struct sdisk_dir_entry self;
    struct sdisk_dir_entry parent;
    memset(&self, 0, sizeof(struct sdisk_dir_entry));
    memset(&parent, 0, sizeof(struct sdisk_dir_entry));
    self.inode = inode_no;
    strncpy(self.name, FS_SELF, SDISK_NAME_LEN);
    parent.inode = parent_inode_no;
    strncpy(parent.name, FS_PARENT, SDISK_NAME_LEN);
    block = malloc(block_size);
    memset(block, 0, block_size);
    memcpy(block, &self, sizeof(struct sdisk_dir_entry));
    memcpy(block + sizeof(struct sdisk_dir_entry),
        &parent, sizeof(struct sdisk_dir_entry));

    char fid[VDISK_FID_LENGTH];
    memset(fid, 0, VDISK_FID_LENGTH);

    return_code = put_block(blk_no, block_size, block,
        block_size, fid, VDISK_FID_LENGTH);

    inode->i_block[0] = superblock.s_free_block;
    return_code = update_inode(inode);
    superblock.s_free_block = next_data_block_no();
    return_code = update_superblock();

    free(block);

    return return_code;
}

/* Find direct relative inode */
static int find_direct_relative_inode(const struct sdisk_inode *base,
        const char *path, struct sdisk_inode *inode) {
fprintf(output, "direct relative inode: %s\n", path);
fflush(output);
    int return_code  = 0;
    int data_blk_idx = 0;
    int inode_found  = 0;
    char *data_buf = (char *)malloc(block_size);
    /* Direct data block reference */
    while ((!inode_found) && (data_blk_idx < SDISK_IND_BLOCK) &&
            base->i_block[data_blk_idx]) {
        return_code = get_block(base->i_block[data_blk_idx], block_size,
            data_buf, block_size);
fprintf(output, "get block: %d\treturn code: %d\n", base->i_block[data_blk_idx], return_code);
fflush(output);

        int offset = 0;
        struct sdisk_dir_entry dir;
        while ((!inode_found) && (offset < block_size)) {
            memcpy((void *)&dir, data_buf + offset,
                sizeof(struct sdisk_dir_entry));
fprintf(output, "data offset: %d\tdir entry name: %s\n", offset, dir.name);
fflush(output);
            /* Find partial entry */
            if (!strncmp(dir.name, path, SDISK_NAME_LEN)) {
fprintf(output, "found indeed.\n");
fflush(output);
                /* Find inode on disk */
                return_code = get_inode(inode, dir.inode);
                inode_found = 1;    /* Inode found */
            } else {
                offset = offset + sizeof(struct sdisk_dir_entry);
            }
        } 
        data_blk_idx++;
    }

    free(data_buf);

    return inode_found;
}

/* Find inode from literal path */
static int find_inode(const char *path, struct sdisk_inode *inode) {
fprintf(output, "find_inode: %s\n", path);
fflush(output);
    int return_code = 0;

    char path_buf[BUFFER_SIZE];         /* Copy of query path */
    memset(path_buf, 0, BUFFER_SIZE);   /* Zero out buffer */
    /* Only relative path is considered */
    strncpy(path_buf, path + 1, BUFFER_SIZE);
fprintf(output, "buffered path: %s\n", path_buf);
fflush(output);
    /* Component to the whole path */
    char *partial_path = NULL;

    /* Find inode */
    struct sdisk_inode pre = root;
    struct sdisk_inode next;
    partial_path = strtok(path_buf, FS_SEPERATOR);

    int inode_found         = 0;        /* Inode still not found */
    int inode_partial_found = 1;        /* Root node is always found */

fprintf(output, "partial path: %s\n", partial_path);
fflush(output);
    while (partial_path && inode_partial_found) {
fprintf(output, "current processing path: %s\n", partial_path);
fflush(output);
        /* Assign found inode */
        next = pre;
        inode_partial_found =
            find_direct_relative_inode(&next, partial_path, &pre);
        
        partial_path = strtok(NULL, FS_SEPERATOR);
    }
    /* Path parsing done and all partial path found */
    /* This can include the root case */
    if (!partial_path && inode_partial_found) {
        inode_found = 1;
    }

    if (inode_found) {
fprintf(output, "I get it!!!!!!!\n");
fflush(output);
        memcpy(inode, &pre, sizeof(struct sdisk_inode));
        return_code = 1;
    } else {
        return_code = 0;
    }

    return return_code;
}

/* Find direct parent inode of literal path */
static int find_parent_inode(const char *path, struct sdisk_inode *parent) {
fprintf(output, "find_parent_inode: %s\n", path);
fflush(output);
    int found = 0;
    char buffer[BUFFER_SIZE];           /* Buffer for partial path */
    strncpy(buffer, path, BUFFER_SIZE);

    if (strncmp(buffer, FS_SEPERATOR, BUFFER_SIZE)) {
        char *p = strrchr(buffer, *FS_SEPERATOR);
        if (p == buffer) {
            *(buffer + 1) = '\0';
        } else {
            *p = '\0';
        }

        found = find_inode(buffer, parent);
    } else {
        memcpy(parent, &root, sizeof(struct sdisk_inode));
    }

    return found;
}

/* Get attribute of files */
static int sdisk_getattr(const char *path, struct stat *stbuf) {
fprintf(output, "path to attr: %s\n", path);
fflush(output);
    struct sdisk_inode inode;
    int err_code = 0;
    int found = find_inode(path, &inode);
fprintf(output, "found: %d\n", found);
fflush(output);
    if (found) {
        /* Set attribute from the inode */
        stbuf->st_mode  = inode.i_mode;     /* Access mode */
        stbuf->st_nlink = inode.i_links_count;
        stbuf->st_size  = inode.i_size;     /* File size(0 for directory) */
        stbuf->st_atime = inode.i_atime;    /* Last access time */
        stbuf->st_ctime = inode.i_ctime;    /* File create time */
        stbuf->st_mtime = inode.i_mtime;    /* File modification time */
    } else {
        err_code = -ENOENT;
    }
fprintf(output, "get attr done.\n");
fflush(output);

    return err_code;
}

/* Read directory entries */
static int sdisk_readdir(const char *path, void *buf,
        fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
fprintf(output, "path to readdir: %s\n", path);
fflush(output);
    struct sdisk_inode inode;
    int found = find_inode(path, &inode);
    if (found) {
        char *data_blk   = (char *)malloc(block_size);
        int data_blk_idx = 0;
        int offset       = 0;
        while (inode.i_block[data_blk_idx]) {
            get_block(inode.i_block[data_blk_idx], block_size,
                data_blk, block_size);
            struct sdisk_dir_entry dir;
            do {
                memcpy((void *)&dir, data_blk + offset,
                    sizeof(struct sdisk_dir_entry));
                if (dir.inode) {
fprintf(output, "\nfile name: %s", dir.name);
fflush(output);
                    filler(buf, dir.name, NULL, 0);
                }
                offset = offset + sizeof(struct sdisk_dir_entry);
            } while (offset < block_size);
            data_blk_idx++;
        }
        free(data_blk);
    } else {
        return -ENOENT;
    }

    return 0;
}

/* Make directory node */
static int sdisk_mkdir(const char *path, mode_t mode) {
fprintf(output, "path to mkdir: %s\n", path);
fflush(output);
    int return_code = 0;
    int found       = 0;
    struct sdisk_inode parent_node;
    struct sdisk_inode inode;
    found = find_parent_inode(path, &parent_node);
fprintf(output, "mkdir find parent: %d\n", found);
fflush(output);
    if (found == 0) {
        return_code = -ENOENT;
    } else {
        char *name = strrchr(path, *FS_SEPERATOR) + 1;
fprintf(output, "mkdir name: %s\n", name);
fflush(output);
        found = find_direct_relative_inode(&parent_node, name, &inode);

        if (found) {
            return_code = -EEXIST;
        } else {
            return_code = new_inode(&inode, superblock.s_free_inode,
                S_IFDIR | ownermode(1, 1, 1) | groupmode(1, 0, 1) |
                othermode(1, 0, 1), 2, block_size);
            return_code = new_empty_dir(&inode, superblock.s_free_block,
                inode.i_inode_no, parent_node.i_inode_no);
            char *data = (char *)malloc(block_size);
            int data_block_no = 0;
            int offset = empty_dir_slot(&parent_node, &data_block_no,
                data, block_size);
fprintf(output, "data block index: %d\toffset: %d\n", data_block_no, offset);
            struct sdisk_dir_entry dir;
            dir.inode = inode.i_inode_no;
            strncpy(dir.name, name, SDISK_NAME_LEN);

            char fid[VDISK_FID_LENGTH];
            memset(fid, 0, VDISK_FID_LENGTH);
            memcpy(data + offset, (void *)&dir, sizeof(struct sdisk_dir_entry));
            return_code = put_block(data_block_no, block_size, data,
                block_size, fid, VDISK_FID_LENGTH);
            free(data);
        }
    }
fprintf(output, "mkdir return code: %d\n", return_code);
fflush(output);

    return return_code;
}

/* Remove directory */
static int sdisk_rmdir(const char *path) {
fprintf(output, "path to mkdir: %s\n", path);
fflush(output);
    int return_code = 0;
    int found       = 0;
    struct sdisk_inode parent_node;
    struct sdisk_inode inode;
    found = find_parent_inode(path, &parent_node);
fprintf(output, "mkdir find parent: %d\n", found);
fflush(output);
    if (found == 0) {
        return_code = -ENOENT;
    } else {
        char *name = strrchr(path, *FS_SEPERATOR) + 1;
fprintf(output, "mkdir name: %s\n", name);
fflush(output);
        found = find_direct_relative_inode(&parent_node, name, &inode);

        if (found) {
            return_code = new_inode(&inode, superblock.s_free_inode,
                S_IFDIR | ownermode(1, 1, 1) | groupmode(1, 0, 1) |
                othermode(1, 0, 1), 2, block_size);
            return_code = new_empty_dir(&inode, superblock.s_free_block,
                inode.i_inode_no, parent_node.i_inode_no);
            char *data = (char *)malloc(block_size);
            int data_block_no = 0;
            int offset = empty_dir_slot(&parent_node, &data_block_no,
                data, block_size);
fprintf(output, "data block index: %d\toffset: %d\n", data_block_no, offset);
            struct sdisk_dir_entry dir;
            dir.inode = inode.i_inode_no;
            strncpy(dir.name, name, SDISK_NAME_LEN);

            char fid[VDISK_FID_LENGTH];
            memset(fid, 0, VDISK_FID_LENGTH);
            memcpy(data + offset, (void *)&dir, sizeof(struct sdisk_dir_entry));
            return_code = put_block(data_block_no, block_size, data,
                block_size, fid, VDISK_FID_LENGTH);
            free(data);
        } else {
            return_code = -ENOENT;
        }
    }
fprintf(output, "mkdir return code: %d\n", return_code);
fflush(output);

    return return_code;
}

/* Update access time */
static int sdisk_utimens(const char *path, const struct timespec tv[2]) {
fprintf(output, "path to utimens: %s\n", path);
fflush(output);
    int return_code = 0;
    struct sdisk_inode inode;
    int found = find_inode(path, &inode);
    if (found) {
        inode.i_atime = tv[0].tv_sec;   /* Update access time */
        inode.i_mtime = tv[1].tv_sec;   /* Update modification time */

        update_inode(&inode);
    } else {
        return_code = -ENOENT;
    }

    return return_code;
}

/* Create new file */
static int sdisk_create(const char *path, mode_t mode,
        struct fuse_file_info *fi) {
fprintf(output, "path to create: %s\n", path);
fflush(output);
    int return_code = 0;
    int found       = 0;
    struct sdisk_inode parent_node;
    struct sdisk_inode inode;
    found = find_parent_inode(path, &parent_node);
fprintf(output, "create find parent: %d\n", found);
fflush(output);
    if (found == 0) {
        return_code = -ENOENT;
    } else {
        char *name = strrchr(path, *FS_SEPERATOR) + 1;
fprintf(output, "create name: %s\n", name);
fflush(output);
        found = find_direct_relative_inode(&parent_node, name, &inode);

        if (found) {
            return_code = -EEXIST;
        } else {
            return_code = new_inode(&inode, superblock.s_free_inode,
                S_IFREG | ownermode(1, 1, 1) | groupmode(1, 0, 0) |
                othermode(1, 0, 0), 1, 0);
            /* When a new file created, only its inode exists */
            /*
            return_code = new_empty_dir(&inode, superblock.s_free_block,
                inode.i_inode_no, parent_node.i_inode_no);
            */
            char *data = (char *)malloc(block_size);
            int data_block_no = 0;
            int offset = empty_dir_slot(&parent_node, &data_block_no,
                data, block_size);
fprintf(output, "data block index: %d\toffset: %d\n", data_block_no, offset);
            struct sdisk_dir_entry dir_entry;
            dir_entry.inode = inode.i_inode_no;
            strncpy(dir_entry.name, name, SDISK_NAME_LEN);

            char fid[VDISK_FID_LENGTH];
            memset(fid, 0, VDISK_FID_LENGTH);
            memcpy(data + offset, (void *)&dir_entry, sizeof(struct sdisk_dir_entry));
            return_code = put_block(data_block_no, block_size, data,
                block_size, fid, VDISK_FID_LENGTH);
            free(data);
        }
    }
fprintf(output, "create return code: %d\n", return_code);
fflush(output);

    return 0;
}

/* Remove file */
static int sdisk_unlink(const char *path) {
fprintf("path to unlink: %s\n", path);
fflush(output);

    return 0;
}

/* File open */
static int sdisk_open(const char *path, struct fuse_file_info *fi) {
fprintf(output, "path to open: %s\n", path);
fflush(output);
    int return_code = 0;
    int found = 0;
    struct sdisk_inode file;

    found = find_inode(path, &file);
    if (found == 0) {
        return_code = -ENOENT;
    }

    return return_code;
}

/* Read data */
static int sdisk_read(const char *path, char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi) {
fprintf(output, "path to read: %s\n", path);
fprintf(output, "offset: %d\tsize: %d\n", offset, size);
fflush(output);
    int return_code = 0;
    int byte_read   = 0;
    struct sdisk_inode file;
    int found = 0;
    found = find_inode(path, &file);
    if (found) {
fprintf(output, "I come for what I want.\n");
fprintf(output, "byte read: %d\tsize: %d\n", byte_read, size);
fflush(output);
        char *remote_data = (char *)malloc(block_size);
        /* Direct reference */
        while (byte_read < size) {
            int cursor      = offset + byte_read;
            int blk_idx     = get_data_blk_idx(&file, cursor);
            int blk_offset  = blockoffset(cursor);
            if (blk_idx) {
                return_code = get_block(blk_idx, block_size, remote_data,
                    block_size);
                memcpy(buf, remote_data, blk_offset);
                byte_read = byte_read + block_size - blk_offset;
            } else {
/* TODO */
/* Error handling */
                break;
            }
        }
        return_code = byte_read;

        free(remote_data);
    } else {
        return_code = -ENOENT;
    }

    return return_code;
}

/* Dump data */
static int sdisk_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi) {
fprintf(output, "path to write: %s\n", path);
fflush(output);

    return 0;
}

static struct fuse_operations sdisk_oper = {
    /* General file access */
    .getattr  = sdisk_getattr,
    .utimens  = sdisk_utimens,
    /* Directory specific operation */
    .mkdir    = sdisk_mkdir,
    .readdir  = sdisk_readdir,
    .rmdir    = sdisk_rmdir,
    /* File specific operation */
    .create   = sdisk_create,
    .unlink   = sdisk_unlink,
    .open     = sdisk_open,
    .read     = sdisk_read,
    .write    = sdisk_write
};

/* Load system configuration */
static void configure(const char *filename);

/* Initialize system, load superblock and inode information */
static void init();

int main(int argc, char **argv) {
output = fopen("output", "w+");
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    /* Load system configuration */
    strncpy(buffer, CONFIG_BASE, BUFFER_SIZE);
    strncat(buffer, FS_SEPERATOR, BUFFER_SIZE);
    strncat(buffer, SYSTEM_CONFIG, BUFFER_SIZE);
    configure(buffer);

    /* Load superblock and inodes */
    init();

    /* Allocate meomry for a block buffer */
    block = malloc(1 << superblock.s_log_block_size);
    
    int error_code = fuse_main(argc, argv, &sdisk_oper, NULL);

    free(block);

fprintf(output, "error code: %d\n", error_code);
fflush(output);
    return error_code;
}

char *strlower(char *str) {
    char *itr = str;

    while (itr && *itr) {
        *itr = *itr | 0x20;
        itr++;
    }

    return str;
}

char *strUpper(char *str) {
    char *itr = str;

    while (itr && *itr) {
        *itr = *itr & 0xdf;
        itr++;
    }

    return str;
}

int getSysConf(char *key) {
    if (!strcmp(key, "DISKS")) {
        return SYS_DISKS;
    } else if (!strncmp(key, "BLOCKSIZE", BUFFER_SIZE)) {
        return SYS_BLOCKSIZE;
    }

    return 0;
}

int getAvailDisk(char *key) {
    if (!strcmp(key, "VDISK")) {
        return AVAIL_VDISK;
    } else if (!strcmp(key, "OSS")) {
        return AVAIL_OSS;
    }

    return -1;
}

/* TODO: need elaborated */
int getDiskConf(char *key) {
    if (!strcmp(key, "ACCOUNT")) {
        return VDISK_ACCOUNT;
    } else if (!strcmp(key, "PASSWD")) {
        return VDISK_PASSWD;
    } else if (!strcmp(key, "ACCOUNTTYPE")) {
        return VDISK_ACCOUNTTYPE;
    } else if (!strcmp(key, "APPKEY")) {
        return VDISK_APP_KEY;
    } else if (!strcmp(key, "APPSECRET")) {
        return VDISK_APPSECRET;
    }

    return -1;
}

void configureDisk(enum Disk disk, char **diskConf) {
    fprintf(stdout, "Load configuration file: %s...\n", diskID[disk]);
    char buffer[BUFFER_SIZE];
    strncpy(buffer, CONFIG_BASE, BUFFER_SIZE);
    strncat(buffer, FS_SEPERATOR, BUFFER_SIZE);
    strncat(buffer, diskID[disk], BUFFER_SIZE);
    strncat(buffer, CONFIG_EXP, BUFFER_SIZE);

    FILE *conf = fopen(buffer, "r");    /* Open configuration file ro */
    if (conf) {
        while (fgets(buffer, BUFFER_SIZE, conf)) {
            trim(buffer);               /* Eliminate heading & tailing ws */
            if (*buffer && *buffer != COMMENT) {
                /* All the setting is stored as `key=value` pairs */
                char *tok = strtok(buffer, "=");
                int conf_index = getSysConf(tok);
                diskConf[conf_index] =
                    (char *)calloc(VALUE_BUFFER_SIZE, sizeof(char));
                tok = strtok(NULL, "=");
                strncpy(diskConf[conf_index], tok, VALUE_BUFFER_SIZE);
            }
        }
    } else {
        fprintf(stderr, "Cannot open file %s. Program exits.\n", buffer);

        exit(SDISK_FILE_OPEN_ERR);
    }

    fclose(conf);
}

void configure(const char *filename) {
    fprintf(stdout, "Load configuration file: %s...\n", filename);
    char buffer[BUFFER_SIZE];
    FILE *conf = fopen(filename, "r");  /* Open configuration file ro */

    if (conf) {
        while (fgets(buffer, BUFFER_SIZE, conf)) {
            trim(buffer);               /* Eliminate heading & tailing ws */
            if (*buffer && *buffer != COMMENT) {
                /* All the setting is stored as `key=value` pairs */
                char *tok = strtok(buffer, "=");
                int conf_index = getSysConf(tok);
                globalSettings[conf_index] =
                    (char *)calloc(VALUE_BUFFER_SIZE, sizeof(char));
                tok = strtok(NULL, "=");
                strncpy(globalSettings[conf_index], tok, VALUE_BUFFER_SIZE);
                if (conf_index == SYS_DISKS) {
                    tok = strtok(tok, ":");
                    strUpper(tok);
                    int disk_index = getAvailDisk(tok);
/* TODO: need elaborated */
                    if (!cloudConfs[disk_index]) {
                        cloudConfs[disk_index] =
                            (char **)calloc(VDISK_ALL, sizeof(char *));
                        configureDisk(disk_index, cloudConfs[disk_index]);
                    }
                }
            }
        }
    } else {
        fprintf(stderr, "Cannot open file %s. Program exits.\n", filename);

        exit(SDISK_FILE_OPEN_ERR);
    }

    fclose(conf);
}

void init() {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    if (access(META, F_OK) != -1) {
        meta = fopen(META, "rb+");      /* Open existing metadata file */ 
    } else {
        meta = fopen(META, "wb+");      /* Create new file */
    }
    if (meta) {
        if (fread((void *)&superblock, 1,
            sizeof(struct sdisk_super_block), meta)) {
            /* Initialize block buffer */
            block_size = 1 << superblock.s_log_block_size;
printf("block size: %d\n", block_size);
            block = malloc(block_size);
            memset(block, 0, block_size);
            get_inode(&root, ROOT_INODE_NR);
        } else {
            if (feof(meta)) {           /* File newly created */
                superblock.s_inodes_count = 0;
                superblock.s_blocks_count = 0;
                superblock.s_free_blocks_count = 0;
                superblock.s_free_inodes_count = 0;
                superblock.s_free_block = ROOT_BLOCK_NR;
                superblock.s_free_inode = ROOT_INODE_NR;
                superblock.s_log_block_size =
                    lsbPos(atol(globalSettings[SYS_BLOCKSIZE])) + 10;
                block_size = 1 << superblock.s_log_block_size;
                superblock.s_mtime = time(NULL);
                superblock.s_wtime = time(NULL);
                superblock.s_mount_count = 1;
                superblock.s_max_mount_count = 0;
                superblock.s_magic = 0xD15C;/* Magic number disk */
                superblock.s_state = 0;
                superblock.s_errors = 0;

                new_inode(&root, superblock.s_free_inode, S_IFDIR |
                    ownermode(1, 1, 1) | groupmode(1, 0, 1) |
                    othermode(1, 0, 1),
                    2, block_size); 

                new_empty_dir(&root, ROOT_BLOCK_NR, ROOT_INODE_NR,
                    ROOT_INODE_NR);
            }
        }
    } else {
        fprintf(stderr, "Fail to access metadata of the file system. "
            "Program exits.\n");

        exit(SDISK_FILE_INODE_ERR);
    }
}
