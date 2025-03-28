#ifndef __KERNEL_INCLUDE_KERNEL_FS_FS_H__
#define __KERNEL_INCLUDE_KERNEL_FS_FS_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <utime.h>

#include <kernel/elf.h>
#include <kernel/core/list.h>
#include <kernel/core/mutex.h>
#include <kernel/core/mailbox.h>
#include <kernel/core/task.h>
#include <kernel/core/semaphore.h>

#define INODE_CACHE_SIZE  32

struct stat;
struct File;
struct FS;

struct Thread;

struct Inode {
  // These two fields never change
  ino_t           ino;
  dev_t           dev;

  // These two fields are protected by inode_cache.lock
  int             ref_count;
  struct KListLink cache_link;

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
  struct KListLink children;
  struct KListLink siblings;

  struct Inode   *inode;
  struct Inode   *mounted;
};

typedef int (*FillDirFunc)(void *, ino_t, const char *, size_t);

struct FSOps {
  char             *name;
  int             (*inode_read)(struct Thread *, struct Inode *);
  int             (*inode_write)(struct Thread *, struct Inode *);
  void            (*inode_delete)(struct Thread *, struct Inode *);
  ssize_t         (*read)(struct Thread *, struct Inode *, uintptr_t, size_t, off_t);
  ssize_t         (*write)(struct Thread *, struct Inode *, uintptr_t, size_t, off_t);
  int             (*rmdir)(struct Thread *, struct Inode *, struct Inode *, const char *);
  ssize_t         (*readdir)(struct Thread *, struct Inode *, void *, FillDirFunc, off_t);
  ssize_t         (*readlink)(struct Thread *, struct Inode *, char *, size_t);
  int             (*create)(struct Thread *, struct Inode *, char *, mode_t, struct Inode **);
  int             (*mkdir)(struct Thread *, struct Inode *, char *, mode_t, struct Inode **);
  int             (*mknod)(struct Thread *, struct Inode *, char *, mode_t, dev_t, struct Inode **);
  int             (*link)(struct Thread *, struct Inode *, char *, struct Inode *);
  int             (*unlink)(struct Thread *, struct Inode *, struct Inode *, const char *);
  struct Inode *  (*lookup)(struct Thread *, struct Inode *, const char *);
  void            (*trunc)(struct Thread *, struct Inode *, off_t);
};

#define FS_MBOX_CAPACITY  4

struct FS {
  dev_t           dev;
  void           *extra;
  struct FSOps   *ops;
  char           *name;
  
  struct KMailBox mbox;
  uint8_t         mbox_buf[FS_MBOX_CAPACITY * sizeof(void *)];
  struct KTask    tasks[FS_MBOX_CAPACITY];
};

#define FS_INODE_VALID  (1 << 0)
#define FS_INODE_DIRTY  (1 << 1)

#define FS_PERM_EXEC    (1 << 0)
#define FS_PERM_WRITE   (1 << 1)
#define FS_PERM_READ    (1 << 2)

#define FS_LOOKUP_REAL          (1 << 0)
#define FS_LOOKUP_FOLLOW_LINKS  (1 << 1)

extern struct PathNode *fs_root;

void          fs_init(void);

int           fs_lookup(const char *, int, struct PathNode **);
int           fs_lookup_inode(const char *, int, struct Inode **);
int           fs_path_lookup(const char *, char *, int, struct PathNode **, struct PathNode **);
int           fs_set_pwd(struct PathNode *);

struct Inode *fs_inode_get(ino_t ino, dev_t dev);
void          fs_inode_put(struct Inode *);
struct Inode *fs_inode_duplicate(struct Inode *);
void          fs_inode_lock(struct Inode *);
int           fs_inode_access(struct Inode *, int);
int           fs_inode_utime(struct Inode *, struct utimbuf *);
void          fs_inode_unlock(struct Inode *);
int           fs_inode_lookup_locked(struct Inode *, const char *, int, struct Inode **);

ssize_t       fs_inode_read_locked(struct Inode *, uintptr_t, size_t, off_t *);
ssize_t       fs_inode_read_dir_locked(struct Inode *, uintptr_t, size_t, off_t *);
int           fs_inode_open_locked(struct Inode *, int, mode_t);
ssize_t       fs_inode_write_locked(struct Inode *, uintptr_t, size_t, off_t *);
ssize_t       fs_inode_getdents(struct Inode *, void *, size_t, off_t *);
int           fs_inode_stat_locked(struct Inode *, struct stat *);
int           fs_create(const char *, mode_t, dev_t, struct PathNode **);
void          fs_inode_cache_init(void);
int           fs_inode_truncate_locked(struct Inode *, off_t length);
int           fs_inode_chmod_locked(struct Inode *, mode_t);
int           fs_inode_ioctl_locked(struct Inode *, int, int);
int           fs_inode_select_locked(struct Inode *, struct timeval *);
int           fs_inode_sync_locked(struct Inode *);
int           fs_inode_chown_locked(struct Inode *, uid_t, gid_t);
ssize_t       fs_inode_readlink(struct Inode *, char *, size_t);
int           fs_permission(struct Inode *, mode_t, int);

// Path operations
int              fs_chmod(const char *, mode_t);
int              fs_open(const char *, int, mode_t, struct File **);
int              fs_link(char *, char *);
int              fs_rename(char *, char *);
int              fs_chdir(const char *);
int              fs_unlink(const char *);
int              fs_rmdir(const char *);
int              fs_access(const char *, int);
ssize_t          fs_readlink(const char *, char *, size_t);
int              fs_utime(const char *, struct utimbuf *);

// File operations
int              fs_close(struct File *);
off_t            fs_seek(struct File *, off_t, int);
ssize_t          fs_read(struct File *, uintptr_t, size_t);
ssize_t          fs_write(struct File *, uintptr_t, size_t);
ssize_t          fs_getdents(struct File *, uintptr_t, size_t);
int              fs_fstat(struct File *, struct stat *);
int              fs_fchdir(struct File *);
int              fs_fchmod(struct File *, mode_t);
int              fs_fchown(struct File *, uid_t, gid_t);
int              fs_ioctl(struct File *, int, int);
int              fs_select(struct File *, struct timeval *);
int              fs_ftruncate(struct File *, off_t);
int              fs_fsync(struct File *);
int              fs_chown(const char *, uid_t, gid_t);

struct PathNode *fs_path_node_create(const char *, struct Inode *, struct PathNode *);
struct PathNode *fs_path_duplicate(struct PathNode *);
void             fs_path_remove(struct PathNode *);
void             fs_path_put(struct PathNode *);
void             fs_path_node_lock(struct PathNode *);
void             fs_path_node_unlock(struct PathNode *);
void             fs_path_lock_two(struct PathNode *, struct PathNode *);
void             fs_path_unlock_two(struct PathNode *, struct PathNode *);
int              fs_path_mount(struct PathNode *, struct Inode *);
int              fs_mount(const char *, const char *);
struct Inode    *fs_path_inode(struct PathNode *);

struct FS *      fs_create_service(char *, dev_t, void *, struct FSOps *);
void             fs_service_task(void *);

struct Thread;

struct FSMessage {
  struct KSemaphore sem;
  struct Thread *sender;

  int type;
  
  union {
    struct {
      struct Inode *inode;
      int r;
    } inode_read;
    struct {
      struct Inode *inode;
    } inode_delete;
    struct {
      struct Inode *inode;
      int r;
    } inode_write;
    struct {
      struct Inode *inode;
      off_t length;
    } trunc;
    struct {
      struct Inode *dir;
      const char *name;
      struct Inode *r;
    } lookup;
    struct {
      struct Inode *dir;
      char *name;
      mode_t mode;
      struct Inode **istore;
      int r;
    } mkdir;
    struct {
      struct Inode *dir;
      char *name;
      mode_t mode;
      struct Inode **istore;
      int r;
    } create;
    struct {
      struct Inode *dir;
      char *name;
      mode_t mode;
      dev_t dev;
      struct Inode **istore;
      int r;
    } mknod;
    struct {
      struct Inode *inode;
      uintptr_t va;
      size_t nbyte;
      off_t off;
      ssize_t r;
    } read;
    struct {
      struct Inode *inode;
      uintptr_t va;
      size_t nbyte;
      off_t off;
      ssize_t r;
    } write;
    struct {
      struct Inode *inode;
      void *buf;
      FillDirFunc func;
      off_t off;
      ssize_t r;
    } readdir;
    struct {
      struct Inode *dir;
      char *name;
      struct Inode *inode;
      int r;
    } link;
    struct {
      struct Inode *dir;
      struct Inode *inode;
      const char *name;
      int r;
    } unlink;
    struct {
      struct Inode *dir;
      struct Inode *inode;
      const char *name;
      int r;
    } rmdir;
  } u;
};

int              fs_send_recv(struct FS *, struct FSMessage *);

enum {
  FS_MSG_INODE_READ   = 1,
  FS_MSG_INODE_DELETE = 2,
  FS_MSG_INODE_WRITE  = 3,
  FS_MSG_TRUNC        = 4,
  FS_MSG_LOOKUP       = 5,
  FS_MSG_MKDIR        = 6,
  FS_MSG_CREATE       = 7,
  FS_MSG_MKNOD        = 8,
  FS_MSG_READ         = 9,
  FS_MSG_WRITE        = 10,
  FS_MSG_READDIR      = 11,
  FS_MSG_LINK         = 12,
  FS_MSG_UNLINK       = 13,
  FS_MSG_RMDIR        = 14,
};

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_FS_H__
