#ifndef __KERNEL_EXT2_H__
#define __KERNEL_EXT2_H__

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
} __attribute__((packed));

#endif  // !__KERNEL_EXT2_H__
