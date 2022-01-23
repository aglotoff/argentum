#ifndef __KERNEL_FS_H__
#define __KERNEL_FS_H__

#include <stdint.h>
#include <sys/types.h>

#include "elf.h"
#include "list.h"
#include "sync.h"

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
};

void          fs_init(void);
struct Inode *fs_name_lookup(const char *);

struct Inode *fs_inode_get(ino_t ino, dev_t dev);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_dup(struct Inode *);
void          fs_inode_lock(struct Inode *);
void          fs_inode_unlock(struct Inode *);
ssize_t       fs_inode_read(struct Inode *, void *, size_t, off_t);
ssize_t       fs_inode_getdents(struct Inode *, void *, size_t, off_t *);
int           fs_inode_stat(struct Inode *, struct stat *);

#endif  // !__KERNEL_FS_H__
