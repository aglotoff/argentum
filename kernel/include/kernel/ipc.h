#ifndef __KERNEL_INCLUDE_KERNEL_IPC_CONNECTION_H__
#define __KERNEL_INCLUDE_KERNEL_IPC_CONNECTION_H__

#include <sys/types.h>
#include <kernel/core/semaphore.h>

struct Inode;
struct stat;

enum {
  CONNECTION_TYPE_FILE = 1,
  CONNECTION_TYPE_PIPE = 2,
  CONNECTION_TYPE_SOCKET = 3,
};

struct File {
  struct KListLink hash_link;
  struct Connection  *connection;
  off_t            offset;       // Current offset within the file
  struct Inode    *inode;
  dev_t            rdev;
};

struct Connection {
  int                  type;         // File type (inode, console, or pipe)
  int                  ref_count;    // The number of references to this file
  
  int                  flags;
  struct PathNode     *node;         // Pointer to the corresponding inode
  
  struct FS           *fs;
};

int             connection_alloc(struct Connection **);
void            connection_init(void);
struct Connection *connection_ref(struct Connection *);
void            connection_unref(struct Connection *);
int             connection_get_flags(struct Connection *);
int             connection_set_flags(struct Connection *, int);

ssize_t         connection_read(struct Connection *, uintptr_t, size_t);
ssize_t         connection_write(struct Connection *, uintptr_t, size_t);
ssize_t         connection_getdents(struct Connection *, uintptr_t, size_t);
int             connection_stat(struct Connection *, struct stat *);
int             connection_chdir(struct Connection *);
off_t           connection_seek(struct Connection *, off_t, int);
int             connection_chmod(struct Connection *, mode_t);
int             connection_chown(struct Connection *, uid_t, gid_t);
int             connection_ioctl(struct Connection *, int, int);
int             connection_select(struct Connection *, struct timeval *);
int             connection_sync(struct Connection *);
int             connection_truncate(struct Connection *, off_t);

struct IpcMessage {
  int type;

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

struct Request {
  uintptr_t send_msg;
  size_t    send_bytes;
  size_t    send_nread;

  uintptr_t recv_msg;
  size_t    recv_bytes;
  size_t    recv_nwrite;

  struct KSemaphore sem;
  struct Process *process;
  struct Connection *connection;
  struct KSpinLock lock;
  int ref_count;

  intptr_t r;
};

#endif  // !__KERNEL_INCLUDE_KERNEL_IPC_CONNECTION__
