#ifndef __KERNEL_INCLUDE_KERNEL_FS_FS_H__
#define __KERNEL_INCLUDE_KERNEL_FS_FS_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/list.h>
#include <kernel/kmutex.h>

#define INODE_CACHE_SIZE  32

struct stat;

struct Inode {
  // These two fields never change
  ino_t           ino;
  dev_t           dev;

  // These two fields are protected by inode_cache.lock
  int             ref_count;
  struct ListLink cache_link;

  struct KMutex   mutex;

  // The following fields (as well as inode contents) are protected by the mutex
  int             flags;
  mode_t          mode;
  nlink_t         nlink;
  uid_t           uid;
  gid_t           gid;
  off_t           size;
  time_t          atime;
  time_t          mtime;
  time_t          ctime;
  dev_t           rdev;

  // Ext2-specific fields
  struct {
    uint32_t        blocks;
    uint32_t        block[15];
  } ext2;
};

#define FS_INODE_VALID  (1 << 0)
#define FS_INODE_DIRTY  (1 << 1)

#define FS_PERM_EXEC    (1 << 0)
#define FS_PERM_WRITE   (1 << 1)
#define FS_PERM_READ    (1 << 2)

extern struct Inode *fs_root;

void          fs_init(void);
int           fs_name_lookup(const char *, int, struct Inode **);

struct Inode *fs_inode_get(ino_t ino, dev_t dev);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_duplicate(struct Inode *);
void          fs_inode_lock(struct Inode *);
void          fs_inode_unlock_put(struct Inode *);
void          fs_inode_unlock(struct Inode *);
int           fs_inode_lookup(struct Inode *, const char *, int, struct Inode **);
int           fs_path_lookup(const char *, char *, int, struct Inode **, struct Inode **);
ssize_t       fs_inode_read(struct Inode *, void *, size_t, off_t *);
ssize_t       fs_inode_read_dir(struct Inode *, void *, size_t, off_t *);
ssize_t       fs_inode_write(struct Inode *, const void *, size_t, off_t *);
ssize_t       fs_inode_getdents(struct Inode *, void *, size_t, off_t *);
int           fs_inode_stat(struct Inode *, struct stat *);
int           fs_create(const char *, mode_t, dev_t, struct Inode **);
void          fs_inode_cache_init(void);
int           fs_inode_truncate(struct Inode *);
int           fs_set_pwd(struct Inode *);

int           fs_unlink(const char *);
int           fs_rmdir(const char *);
int           fs_permission(struct Inode *, mode_t, int);
int           fs_link(char *, char *);
int           fs_chdir(const char *);
int           fs_chmod(const char *path, mode_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_FS_H__
