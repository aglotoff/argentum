#ifndef __KERNEL_FS_FS_H__
#define __KERNEL_FS_FS_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/list.h>
#include <kernel/sync.h>

#define INODE_CACHE_SIZE  32

struct stat;

struct Inode {
  ino_t           ino;
  dev_t           dev;
  int             valid;
  int             ref_count;
  struct ListLink cache_link;
  struct Mutex    mutex;
  struct ListLink wait_queue;
  
  // FS-independent data
  mode_t          mode;
  nlink_t         nlink;
  uid_t           uid;
  gid_t           gid;
  off_t           size;
  time_t          atime;
  time_t          mtime;
  time_t          ctime;

  // Ext2-specific data
  uint32_t        blocks;
  uint32_t        block[15];
  uint8_t         major;
  uint8_t         minor;
};

int           fs_block_alloc(dev_t, uint32_t *);
void          fs_block_free(dev_t, uint32_t);

void          fs_init(void);
int           fs_name_lookup(const char *, struct Inode **);

struct Inode *fs_inode_alloc(mode_t, dev_t);
struct Inode *fs_inode_get(ino_t ino, dev_t dev);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_dup(struct Inode *);
void          fs_inode_update(struct Inode *);
void          fs_inode_lock(struct Inode *);
void          fs_inode_unlock(struct Inode *);
struct Inode *fs_dir_lookup(struct Inode *, const char *);
int           fs_dir_link(struct Inode *, char *, unsigned, mode_t);
int           fs_path_lookup(const char *, char *, int, struct Inode **);
ssize_t       fs_inode_read(struct Inode *, void *, size_t, off_t);
ssize_t       fs_inode_write(struct Inode *, const void *, size_t, off_t);
ssize_t       fs_inode_getdents(struct Inode *, void *, size_t, off_t *);
int           fs_inode_stat(struct Inode *, struct stat *);
int           fs_create(const char *, mode_t, dev_t, struct Inode **);
void          fs_inode_cache_init(void);

#endif  // !__KERNEL_FS_FS_H__
