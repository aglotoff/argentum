#ifndef __KERNEL_INCLUDE_KERNEL_FS_FS_H__
#define __KERNEL_INCLUDE_KERNEL_FS_FS_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/list.h>
#include <kernel/mutex.h>

#define INODE_CACHE_SIZE  32

struct stat;

struct FS;

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

  struct FS      *fs;
  void           *extra;
};

struct PathNode {
  char            name[NAME_MAX + 1];
  int             ref_count;

  struct KMutex   mutex;

  struct PathNode    *parent;
  struct ListLink children;
  struct ListLink siblings;

  struct Inode   *inode;
  struct Inode   *mounted;
};

typedef int (*FillDirFunc)(void *, ino_t, const char *, size_t);

struct FSOps {
  int             (*inode_read)(struct Inode *);
  int             (*inode_write)(struct Inode *);
  void            (*inode_delete)(struct Inode *);
  ssize_t         (*read)(struct Inode *, void *, size_t, off_t);
  ssize_t         (*write)(struct Inode *, const void *, size_t, off_t);
  int             (*rmdir)(struct Inode *, struct Inode *);
  ssize_t         (*readdir)(struct Inode *, void *, FillDirFunc, off_t);
  int             (*create)(struct Inode *, char *, mode_t, struct Inode **);
  int             (*mkdir)(struct Inode *, char *, mode_t, struct Inode **);
  int             (*mknod)(struct Inode *, char *, mode_t, dev_t, struct Inode **);
  int             (*link)(struct Inode *, char *, struct Inode *);
  int             (*unlink)(struct Inode *, struct Inode *);
  struct Inode *  (*lookup)(struct Inode *, const char *);
  void            (*trunc)(struct Inode *, off_t);
};

struct FS {
  dev_t         dev;
  void         *extra;
  struct FSOps *ops;
};

#define FS_INODE_VALID  (1 << 0)
#define FS_INODE_DIRTY  (1 << 1)

#define FS_PERM_EXEC    (1 << 0)
#define FS_PERM_WRITE   (1 << 1)
#define FS_PERM_READ    (1 << 2)

extern struct PathNode *fs_root;

void          fs_init(void);
int           fs_lookup(const char *, int, struct PathNode **);
int           fs_path_lookup(const char *, char *, int, struct PathNode **, struct PathNode **);

struct Inode *fs_inode_get(ino_t ino, dev_t dev);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_duplicate(struct Inode *);
void          fs_inode_lock(struct Inode *);
int           fs_inode_access(struct Inode *, int);
void          fs_inode_unlock(struct Inode *);
int           fs_inode_lookup(struct Inode *, const char *, int, struct Inode **);

ssize_t       fs_inode_read(struct Inode *, void *, size_t, off_t *);
ssize_t       fs_inode_read_dir(struct Inode *, void *, size_t, off_t *);
ssize_t       fs_inode_write(struct Inode *, const void *, size_t, off_t *);
ssize_t       fs_inode_getdents(struct Inode *, void *, size_t, off_t *);
int           fs_inode_stat(struct Inode *, struct stat *);
int           fs_create(const char *, mode_t, dev_t, struct PathNode **);
void          fs_inode_cache_init(void);
int           fs_inode_truncate(struct Inode *);
int           fs_inode_chmod(struct Inode *, mode_t);
int           fs_inode_ioctl(struct Inode *, int, int);
int           fs_set_pwd(struct PathNode *);

int           fs_unlink(const char *);
int           fs_rmdir(const char *);
int           fs_permission(struct Inode *, mode_t, int);
int           fs_link(char *, char *);
int           fs_chdir(const char *);
int           fs_access(const char *, int);

struct PathNode *fs_path_create(const char *, struct Inode *, struct PathNode *);
struct PathNode *fs_path_duplicate(struct PathNode *);
void             fs_path_remove(struct PathNode *);
void             fs_path_put(struct PathNode *);
void             fs_path_lock(struct PathNode *);
void             fs_path_unlock(struct PathNode *);
void             fs_path_lock_two(struct PathNode *, struct PathNode *);
void             fs_path_unlock_two(struct PathNode *, struct PathNode *);

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_FS_H__
