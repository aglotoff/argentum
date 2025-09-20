#ifndef __KERNEL_INCLUDE_KERNEL_IPC_CHANNEL_H__
#define __KERNEL_INCLUDE_KERNEL_IPC_CHANNEL_H__

#include <sys/types.h>
#include <kernel/core/semaphore.h>

struct Inode;
struct stat;

enum {
  CHANNEL_TYPE_FILE = 1,
  CHANNEL_TYPE_PIPE = 2,
  CHANNEL_TYPE_SOCKET = 3,
};

struct File {
  struct KListLink hash_link;
  struct Channel  *channel;
  off_t            offset;       // Current offset within the file
  struct Inode    *inode;
  dev_t            rdev;
};

struct Channel {
  int                  type;         // File type (inode, console, or pipe)
  int                  ref_count;    // The number of references to this file
  
  int                  flags;
  struct PathNode     *node;         // Pointer to the corresponding inode
  
  struct FS           *fs;
};

int             channel_alloc(struct Channel **);
void            channel_init(void);
struct Channel *channel_ref(struct Channel *);
void            channel_unref(struct Channel *);
int             channel_get_flags(struct Channel *);
int             channel_set_flags(struct Channel *, int);

ssize_t         channel_read(struct Channel *, uintptr_t, size_t);
ssize_t         channel_write(struct Channel *, uintptr_t, size_t);
ssize_t         channel_getdents(struct Channel *, uintptr_t, size_t);
int             channel_stat(struct Channel *, struct stat *);
int             channel_chdir(struct Channel *);
off_t           channel_seek(struct Channel *, off_t, int);
int             channel_chmod(struct Channel *, mode_t);
int             channel_chown(struct Channel *, uid_t, gid_t);
int             channel_ioctl(struct Channel *, int, int);
int             channel_select(struct Channel *, struct timeval *);
int             channel_sync(struct Channel *);
int             channel_truncate(struct Channel *, off_t);

struct IpcMessage {
  int type;
  uintptr_t r;
  
  union {
    struct {
      ino_t ino;
      int amode;
    } access;
    struct {
      ino_t ino;
    } chdir;
    struct {
      ino_t ino;
      mode_t mode;
    } chmod;
    struct {
      ino_t ino;
      uid_t uid;
      gid_t gid;
    } chown;
    struct {
      ino_t dir_ino;
      char *name;
      mode_t mode;
      dev_t dev;
      ino_t *istore;
    } create;
    struct {
      ino_t dir_ino;
      char *name;
      ino_t ino;
    } link;
    struct {
      ino_t dir_ino;
      const char *name;
      ino_t *istore;
      int flags;
    } lookup;
    struct {
      ino_t ino;
      uintptr_t va;
      size_t nbyte;
    } readlink;
    struct {
      ino_t dir_ino;
      ino_t ino;
      const char *name;
    } rmdir;
    struct {
      ino_t ino;
      struct stat *buf;
    } stat;
    struct {
      ino_t dir_ino;
      char *name;
      mode_t mode;
      const char *path;
      ino_t *istore;
    } symlink;
    struct {
      ino_t dir_ino;
      ino_t ino;
      const char *name;
    } unlink;
    struct {
      ino_t ino;
      struct utimbuf *times;
    } utime;

    struct {
    } close;
    struct {
      mode_t mode;
    } fchmod;
    struct {
      uid_t uid;
      gid_t gid;
    } fchown;
    struct {
      struct stat *buf;
    } fstat;
    struct {
    } fsync;
    struct {
      int request;
      int arg;
    } ioctl;
    struct {
      ino_t ino;
      int oflag;
      mode_t mode;
    } open;
    struct {
      uintptr_t va;
      size_t nbyte;
    } read;
    struct {
      uintptr_t va;
      size_t nbyte;
    } readdir;
    struct {
      off_t offset;
      int whence;
    } seek;
    struct {
      struct timeval *timeout;
    } select;
    struct {
      off_t length;
    } trunc;
    struct {
      uintptr_t va;
      size_t nbyte;
    } write;
  } u;
};

enum {
  IPC_MSG_ACCESS,
  IPC_MSG_CHDIR,
  IPC_MSG_CHMOD,
  IPC_MSG_CHOWN,
  IPC_MSG_CREATE,
  IPC_MSG_LINK,
  IPC_MSG_LOOKUP,
  IPC_MSG_STAT,
  IPC_MSG_READLINK,
  IPC_MSG_RMDIR,
  IPC_MSG_SYMLINK,
  IPC_MSG_UNLINK,
  IPC_MSG_UTIME,
  
  IPC_MSG_CLOSE,
  IPC_MSG_FCHMOD,
  IPC_MSG_FCHOWN,
  IPC_MSG_FSTAT,
  IPC_MSG_FSYNC,
  IPC_MSG_IOCTL,
  IPC_MSG_OPEN,
  IPC_MSG_READ,
  IPC_MSG_READDIR,
  IPC_MSG_SEEK,
  IPC_MSG_SELECT,
  IPC_MSG_TRUNC,
  IPC_MSG_WRITE,
};

struct IpcRequest {
  struct IpcMessage *msg;
  struct KSemaphore sem;
  struct Thread *sender;
  struct Channel *channel;
  struct KSpinLock lock;
  int ref_count;
};

#endif  // !__KERNEL_INCLUDE_KERNEL_IPC_CHANNEL__
