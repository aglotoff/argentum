#ifndef __KERNEL_FS_FS_H__
#define __KERNEL_FS_FS_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>
#include <sys/types.h>

#include <elf.h>
#include <list.h>
#include <mutex.h>

#define INODE_CACHE_SIZE  32

struct stat;

struct Inode {
  ino_t           ino;
  dev_t           dev;
  int             flags;
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
  dev_t           rdev;

  // Ext2-specific data
  uint32_t        blocks;
  uint32_t        block[15];
};

#define FS_INODE_VALID  (1 << 0)
#define FS_INODE_DIRTY  (1 << 1)

#define FS_PERM_EXEC    (1 << 0)
#define FS_PERM_WRITE   (1 << 1)
#define FS_PERM_READ    (1 << 2)

extern struct Inode **fs_root;

void          fs_init(void);
int           fs_name_lookup(const char *, struct Inode **);

struct Inode *fs_inode_get(ino_t ino, dev_t dev);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_dup(struct Inode *);
void          fs_inode_lock(struct Inode *);
void          fs_inode_unlock_put(struct Inode *);
void          fs_inode_unlock(struct Inode *);
int           fs_path_lookup(const char *, char *, int, struct Inode **);
ssize_t       fs_inode_read(struct Inode *, void *, size_t, off_t *);
ssize_t       fs_inode_write(struct Inode *, const void *, size_t, off_t *);
ssize_t       fs_inode_getdents(struct Inode *, void *, size_t, off_t *);
int           fs_inode_stat(struct Inode *, struct stat *);
int           fs_create(const char *, mode_t, dev_t, struct Inode **);
void          fs_inode_cache_init(void);
int           fs_inode_trunc(struct Inode *);
int           fs_unlink(const char *);
int           fs_rmdir(const char *);
int           fs_permissions(struct Inode *, mode_t);
int           fs_link(char *, char *);
int           fs_chdir(struct Inode *);
int           fs_chmod(struct Inode *, mode_t);

#endif  // !__KERNEL_FS_FS_H__
