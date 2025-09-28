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

#define INODE_CACHE_SIZE  32

struct stat;
struct Channel;
struct FS;

struct Process;

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
  int             (*inode_read)(struct Process *, struct Inode *);
  int             (*inode_write)(struct Process *, struct Inode *);
  void            (*inode_delete)(struct Process *, struct Inode *);
  ssize_t         (*read)(struct Process *, struct Inode *, uintptr_t, size_t, off_t);
  ssize_t         (*write)(struct Process *, struct Inode *, uintptr_t, size_t, off_t);
  int             (*rmdir)(struct Process *, struct Inode *, struct Inode *, const char *);
  ssize_t         (*readdir)(struct Process *, struct Inode *, void *, FillDirFunc, off_t);
  ssize_t         (*readlink)(struct Process *, struct Inode *, uintptr_t, size_t);
  int             (*create)(struct Process *, struct Inode *, char *, mode_t, struct Inode **);
  int             (*symlink)(struct Process *, struct Inode *, char *, mode_t, const char *, struct Inode **);
  int             (*mkdir)(struct Process *, struct Inode *, char *, mode_t, struct Inode **);
  int             (*mknod)(struct Process *, struct Inode *, char *, mode_t, dev_t, struct Inode **);
  int             (*link)(struct Process *, struct Inode *, char *, struct Inode *);
  int             (*unlink)(struct Process *, struct Inode *, struct Inode *, const char *);
  struct Inode *  (*lookup)(struct Process *, struct Inode *, const char *);
  void            (*trunc)(struct Process *, struct Inode *, off_t);
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
int              fs_inode_permission(struct Process *, struct Inode *, mode_t, int);
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
int              fs_symlink(const char *, const char *);
int              fs_utime(const char *, struct utimbuf *);

// File operations
int              fs_close(struct Channel *);
int              fs_select(struct Channel *, struct timeval *);

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

struct IpcMessage;

// TODO: move to more appropriate place!
intptr_t         fs_send_recv(struct Channel *, void *, size_t, void *, size_t);
ssize_t          ipc_request_write(struct IpcRequest *, void *, size_t);
ssize_t          ipc_request_read(struct IpcRequest *, void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_H__
