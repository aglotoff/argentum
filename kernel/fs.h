#ifndef __KERNEL_FS_H__
#define __KERNEL_FS_H__

#include <stdint.h>
#include <sys/types.h>

#include "elf.h"
#include "ext2.h"
#include "list.h"
#include "sync.h"

#define INODE_CACHE_SIZE  32

struct Inode {
  unsigned         num;
  int              valid;
  int              ref_count;
  struct ListLink  cache_link;
  struct Mutex     mutex;
  struct ListLink  wait_queue;
  
  struct Ext2Inode data;
};

void          fs_init(void);
struct Inode *fs_name_lookup(const char *);

struct Inode *fs_inode_get(unsigned inum);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_dup(struct Inode *);
void          fs_inode_lock(struct Inode *);
void          fs_inode_unlock(struct Inode *);
ssize_t       fs_inode_read(struct Inode *, void *, size_t, off_t);

#endif  // !__KERNEL_FS_H__
