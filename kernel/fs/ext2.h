#ifndef __KERNEL_FS_EXT2_H__
#define __KERNEL_FS_EXT2_H__

#include <stdint.h>
#include <sys/types.h>

#include <kernel/kmutex.h>
#include <kernel/fs/fs.h>

/**
 * Filesystem Superblock
 */
struct Ext2Superblock {
  /** The total number of inodes, both used and free */
  uint32_t inodes_count;
  /** The total number of blocks, including all used, free, and reserved */
  uint32_t block_count;
  /** The total number of block reserved for the super user */
  uint32_t r_blocks_count;
  /** The total number of free blocks, including reserved blocks */
  uint32_t free_blocks_count;
  /** The total number of free inodes */
  uint32_t free_inodes_count;
  /** ID of the block containing the superblock structure */
  uint32_t first_data_block;
 
  uint32_t log_block_size;
  uint32_t log_frag_size;
  uint32_t blocks_per_group;
  uint32_t frags_per_group;
  uint32_t inodes_per_group;
  uint32_t mtime;
  uint32_t wtime;
  uint16_t mnt_count;
  uint16_t max_mnt_count;
  uint16_t magic;
  uint16_t state;
  uint16_t errors;
  uint16_t minor_rev_level;
  uint32_t lastcheck;
  uint32_t checkinterval;
  uint32_t creator_os;
  uint32_t rev_level;
  uint16_t def_resuid;
  uint16_t def_resgid;
  uint32_t first_ino;
  uint16_t inode_size;
  uint16_t block_group_nr;
} __attribute__((packed));

struct Ext2SuperblockData {
  struct KMutex mutex;

  uint32_t inodes_count;
  uint32_t block_count;
  uint32_t r_blocks_count;
  uint32_t free_blocks_count;
  uint32_t log_block_size;
  uint32_t blocks_per_group;
  uint32_t inodes_per_group;
  uint32_t wtime;
  uint16_t inode_size;

  uint32_t block_size;
};

/**
 * Block Group Descriptor 
 */
struct Ext2BlockGroup {
  /** ID of the first block of the block bitmap for this group */
  uint32_t block_bitmap;
  /** ID of the first block of the inode bitmap for this group */
  uint32_t inode_bitmap;
  /** ID of the first block of the inode table for this group */
  uint32_t inode_table;
  /** The total number of free blocks for this group */
  uint16_t free_blocks_count;
  /** The total number of free inodes for this group */
  uint16_t free_inodes_count;
  /** The total number of inodes allocated to directories for this group */
  uint16_t used_dirs_count;
  /** Padding to 32-bit boundary */
  uint16_t pad;
  /** Reserved space for future revisions */
  uint8_t  reserved[12];
} __attribute__((packed));

struct Ext2Inode {
  uint16_t mode;
  uint16_t uid;
  uint32_t size;
  uint32_t atime;
  uint32_t ctime;
  uint32_t mtime;
  uint32_t dtime;
  uint16_t gid;
  uint16_t links_count;
  uint32_t blocks;
  uint32_t flags;
  uint8_t  osd1[4];
  uint32_t block[15];
  uint32_t generation;
  uint32_t file_acl;
  uint32_t dir_acl;
  uint32_t faddr;
  uint8_t  osd2[12];
} __attribute__((packed));

struct Ext2InodeExtra {
  uint32_t        blocks;
  uint32_t        block[15];
};

// File format
#define EXT2_S_IFMASK   (0xF << 12)
#define EXT2_S_IFIFO    (0x1 << 12)
#define EXT2_S_IFCHR    (0x2 << 12)
#define EXT2_S_IFDIR    (0x4 << 12)
#define EXT2_S_IFBLK    (0x6 << 12)
#define EXT2_S_IFREG    (0x8 << 12)
#define EXT2_S_IFLINK   (0xA << 12)
#define EXT2_S_IFSOCK   (0xC << 12)

// Process execution user/group override
#define EXT2_S_ISUID    (1 << 11)
#define EXT2_S_ISGID    (1 << 10)
#define EXT2_S_ISVTX    (1 << 9)

// Access rights
#define EXT2_S_IRUSR    (1 << 8)
#define EXT2_S_IWUSR    (1 << 7)
#define EXT2_S_IXUSR    (1 << 6)
#define EXT2_S_IRGRP    (1 << 5)
#define EXT2_S_IWGRP    (1 << 4)
#define EXT2_S_IXGRP    (1 << 3)
#define EXT2_S_IROTH    (1 << 2)
#define EXT2_S_IWOTH    (1 << 1)
#define EXT2_S_IXOTH    (1 << 0)

struct Ext2DirEntry {
  uint32_t inode;
  uint16_t rec_len;
  uint8_t  name_len;
  uint8_t  file_type;
  char     name[255];
} __attribute__((packed));

#define EXT2_FT_UNKNOWN   0
#define EXT2_FT_REG_FILE  1
#define EXT2_FT_DIR       2
#define EXT2_FT_CHRDEV    3
#define EXT2_FT_BLKDEV    4
#define EXT2_FT_FIFO      5
#define EXT2_FT_SOCK      6
#define EXT2_FT_SYMLINK   7

struct Inode;

extern struct FS ext2fs;

int           ext2_bitmap_alloc(struct Ext2SuperblockData *, uint32_t, size_t, dev_t, uint32_t *);
int           ext2_bitmap_free(struct Ext2SuperblockData *, uint32_t, dev_t, uint32_t);

int           ext2_block_alloc(struct Ext2SuperblockData *, dev_t, uint32_t *);
void          ext2_block_free(struct Ext2SuperblockData *, dev_t, uint32_t);
int           ext2_block_zero(struct Ext2SuperblockData *, uint32_t, uint32_t);

int           ext2_inode_alloc(struct Ext2SuperblockData *, mode_t, dev_t, dev_t, uint32_t *, uint32_t);
void          ext2_inode_free(struct Ext2SuperblockData *, dev_t, uint32_t);

void          ext2_sb_sync(struct Ext2SuperblockData *, dev_t);
struct Inode *ext2_mount(dev_t);
int           ext2_inode_read(struct Inode *);
int           ext2_inode_writee(struct Inode *);
void          ext2_inode_delete(struct Inode *);

int           ext2_create(struct Inode *, char *, mode_t,
                                struct Inode **);
struct Inode *ext2_lookup(struct Inode *, const char *);
int           ext2_link(struct Inode *, char *, struct Inode *);
int           ext2_unlink(struct Inode *, struct Inode *);
int           ext2_mkdir(struct Inode *, char *, mode_t, struct Inode **);
int           ext2_rmdir(struct Inode *, struct Inode *);
int           ext2_mknod(struct Inode *, char *, mode_t, dev_t, struct Inode **);
void          ext2_trunc(struct Inode *, off_t);
ssize_t       ext2_read(struct Inode *, void *, size_t, off_t);
ssize_t       ext2_write(struct Inode *, const void *, size_t, off_t);

ssize_t       ext2_readdir(struct Inode *, void *, FillDirFunc, off_t);
uint32_t      ext2_inode_get_block(struct Inode *, uint32_t, int);

#endif  // !__KERNEL_FS_EXT2_H__
