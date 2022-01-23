#ifndef __KERNEL_EXT2_H__
#define __KERNEL_EXT2_H__

#include <stdint.h>

struct Ext2Superblock {
  uint32_t inodes_count;
  uint32_t block_count;
  uint32_t r_blocks_count;
  uint32_t free_blocks_count;
  uint32_t free_inodes_count;
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

struct Ext2GroupDesc {
  uint32_t block_bitmap;
  uint32_t inode_bitmap;
  uint32_t inode_table;
  uint16_t free_blocks_count;
  uint16_t free_inodes_count;
  uint16_t used_dirs_count;
  uint16_t pad;
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
  char     name[256];
} __attribute__((packed));

#endif  // !__KERNEL_EXT2_H__
