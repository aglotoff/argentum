#ifndef __KERNEL_INCLUDE_KERNEL_FS_H__
#define __KERNEL_INCLUDE_KERNEL_FS_H__

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
struct Channel;
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
  char             name[NAME_MAX + 1];
  int              ref_count;

  struct KMutex    mutex;

  struct PathNode *parent;
  struct KListLink children;
  struct KListLink siblings;

  // struct Inode    *inode;
  // struct Inode    *mounted;

  ino_t            ino;
  struct Channel  *channel;

  ino_t            mounted_ino;
  struct Channel  *mounted_channel;
};

typedef int (*FillDirFunc)(void *, ino_t, const char *, size_t);

struct FSOps {
  char             *name;
  struct Inode *  (*inode_get)(struct FS *, ino_t);
  int             (*inode_read)(struct Thread *, struct Inode *);
  int             (*inode_write)(struct Thread *, struct Inode *);
  void            (*inode_delete)(struct Thread *, struct Inode *);
  ssize_t         (*read)(struct Thread *, struct Inode *, uintptr_t, size_t, off_t);
  ssize_t         (*write)(struct Thread *, struct Inode *, uintptr_t, size_t, off_t);
  int             (*rmdir)(struct Thread *, struct Inode *, struct Inode *, const char *);
  ssize_t         (*readdir)(struct Thread *, struct Inode *, void *, FillDirFunc, off_t);
  ssize_t         (*readlink)(struct Thread *, struct Inode *, uintptr_t, size_t);
  int             (*create)(struct Thread *, struct Inode *, char *, mode_t, struct Inode **);
  int             (*mkdir)(struct Thread *, struct Inode *, char *, mode_t, struct Inode **);
  int             (*mknod)(struct Thread *, struct Inode *, char *, mode_t, dev_t, struct Inode **);
  int             (*link)(struct Thread *, struct Inode *, char *, struct Inode *);
  int             (*unlink)(struct Thread *, struct Inode *, struct Inode *, const char *);
  struct Inode *  (*lookup)(struct Thread *, struct Inode *, const char *);
  void            (*trunc)(struct Thread *, struct Inode *, off_t);
};

#define FS_MBOX_CAPACITY  20

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

void             fs_init(void);

struct Inode    *fs_inode_get(ino_t ino, dev_t dev);
void             fs_inode_put(struct Inode *);
struct Inode    *fs_inode_duplicate(struct Inode *);
void             fs_inode_lock(struct Inode *);
void             fs_inode_unlock(struct Inode *);
void             fs_inode_cache_init(void);
int              fs_inode_permission(struct Thread *, struct Inode *, mode_t, int);
void             fs_inode_lock_two(struct Inode *, struct Inode *);
void             fs_inode_unlock_two(struct Inode *, struct Inode *);

// Path operations
int              fs_chmod(const char *, mode_t);
int              fs_chown(const char *, uid_t, gid_t);
int              fs_create(const char *, mode_t, dev_t, struct PathNode **);
int              fs_open(const char *, int, mode_t, struct Channel **);
int              fs_link(char *, char *);
int              fs_rename(char *, char *);
int              fs_chdir(const char *);
int              fs_unlink(const char *);
int              fs_rmdir(const char *);
int              fs_access(const char *, int);
ssize_t          fs_readlink(const char *, uintptr_t, size_t);
int              fs_utime(const char *, struct utimbuf *);

// File operations
int              fs_close(struct Channel *);
off_t            fs_seek(struct Channel *, off_t, int);
ssize_t          fs_read(struct Channel *, uintptr_t, size_t);
ssize_t          fs_write(struct Channel *, uintptr_t, size_t);
ssize_t          fs_getdents(struct Channel *, uintptr_t, size_t);
int              fs_fstat(struct Channel *, struct stat *);
int              fs_fchdir(struct Channel *);
int              fs_fchmod(struct Channel *, mode_t);
int              fs_fchown(struct Channel *, uid_t, gid_t);
int              fs_ioctl(struct Channel *, int, int);
int              fs_select(struct Channel *, struct timeval *);
int              fs_ftruncate(struct Channel *, off_t);
int              fs_fsync(struct Channel *);

struct PathNode *fs_path_node_create(const char *, ino_t, struct Channel *, struct PathNode *);
struct PathNode *fs_path_node_ref(struct PathNode *);
void             fs_path_node_remove(struct PathNode *);
void             fs_path_node_unref(struct PathNode *);
void             fs_path_node_lock(struct PathNode *);
void             fs_path_node_unlock(struct PathNode *);
void             fs_path_lock_two(struct PathNode *, struct PathNode *);
void             fs_path_unlock_two(struct PathNode *, struct PathNode *);
int              fs_mount(const char *, const char *);
int              fs_path_resolve(const char *, int, struct PathNode **);
int              fs_path_node_resolve(const char *, char *, int, struct PathNode **, struct PathNode **);
int              fs_path_set_cwd(struct PathNode *);
ino_t            fs_path_ino(struct PathNode *, struct Channel **);

struct FS *      fs_create_service(char *, dev_t, void *, struct FSOps *);
void             fs_service_task(void *);

struct Thread;

struct FSMessage {
  struct KSemaphore sem;
  struct Thread *sender;
  struct Channel *channel;

  int type;
  
  union {
    struct {
      ino_t ino;
      int amode;
      int r;
    } access;
    struct {
      ino_t ino;
      int r;
    } chdir;
    struct {
      ino_t ino;
      mode_t mode;
      int r;
    } chmod;
    struct {
      ino_t ino;
      uid_t uid;
      gid_t gid;
      int r;
    } chown;
    struct {
      ino_t dir_ino;
      char *name;
      mode_t mode;
      dev_t dev;
      ino_t *istore;
      int r;
    } create;
    struct {
      ino_t dir_ino;
      char *name;
      ino_t ino;
      int r;
    } link;
    struct {
      ino_t dir_ino;
      const char *name;
      ino_t *istore;
      int flags;
      int r;
    } lookup;
    struct {
      ino_t ino;
      uintptr_t va;
      size_t nbyte;
      ssize_t r;
    } readlink;
    struct {
      ino_t dir_ino;
      ino_t ino;
      const char *name;
      int r;
    } rmdir;
    struct {
      ino_t ino;
      struct stat *buf;
      int r;
    } stat;
    struct {
      ino_t dir_ino;
      ino_t ino;
      const char *name;
      int r;
    } unlink;
    struct {
      ino_t ino;
      struct utimbuf *times;
      int r;
    } utime;

    struct {
      int r;
    } close;
    struct {
      mode_t mode;
      int r;
    } fchmod;
    struct {
      uid_t uid;
      gid_t gid;
      int r;
    } fchown;
    struct {
      struct stat *buf;
      int r;
    } fstat;
    struct {
      int r;
    } fsync;
    struct {
      int request;
      int arg;
      int r;
    } ioctl;
    struct {
      ino_t ino;
      int oflag;
      int r;
    } open;
    struct {
      uintptr_t va;
      size_t nbyte;
      ssize_t r;
    } read;
    struct {
      uintptr_t va;
      size_t nbyte;
      ssize_t r;
    } readdir;
    struct {
      off_t offset;
      int whence;
      off_t r;
    } seek;
    struct {
      struct timeval *timeout;
      int r;
    } select;
    struct {
      off_t length;
      int r;
    } trunc;
    struct {
      uintptr_t va;
      size_t nbyte;
      ssize_t r;
    } write;
  } u;
};

int              fs_send_recv(struct Channel *, struct FSMessage *);

enum {
  FS_MSG_ACCESS,
  FS_MSG_CHDIR,
  FS_MSG_CHMOD,
  FS_MSG_CHOWN,
  FS_MSG_CREATE,
  FS_MSG_LINK,
  FS_MSG_LOOKUP,
  FS_MSG_STAT,
  FS_MSG_READLINK,
  FS_MSG_RMDIR,
  FS_MSG_UNLINK,
  FS_MSG_UTIME,
  
  FS_MSG_CLOSE,
  FS_MSG_FCHMOD,
  FS_MSG_FCHOWN,
  FS_MSG_FSTAT,
  FS_MSG_FSYNC,
  FS_MSG_IOCTL,
  FS_MSG_OPEN,
  FS_MSG_READ,
  FS_MSG_READDIR,
  FS_MSG_SEEK,
  FS_MSG_SELECT,
  FS_MSG_TRUNC,
  FS_MSG_WRITE,
};

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_H__
