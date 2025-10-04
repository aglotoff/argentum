#ifndef _SYS_IPC_H
#define _SYS_IPC_H

#include <sys/types.h>
#include <sys/utime.h>
#include <sys/uio.h>

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
    } create;
    struct {
      ino_t dir_ino;
      char *name;
      ino_t ino;
    } link;
    struct {
      ino_t dir_ino;
      const char *name;
      int flags;
    } lookup;
    struct {
      ino_t ino;
      size_t nbyte;
    } readlink;
    struct {
      ino_t dir_ino;
      ino_t ino;
      const char *name;
    } rmdir;
    struct {
      ino_t ino;
    } stat;
    struct {
      ino_t dir_ino;
      char *name;
      mode_t mode;
      const char *path;
    } symlink;
    struct {
      ino_t dir_ino;
      ino_t ino;
      const char *name;
    } unlink;
    struct {
      ino_t ino;
      struct utimbuf times;
    } utime;

    struct {
      mode_t mode;
    } fchmod;
    struct {
      uid_t uid;
      gid_t gid;
    } fchown;
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
      size_t nbyte;
    } read;
    struct {
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

intptr_t ipc_send(int, void *, size_t, void *, size_t);
intptr_t ipc_sendv(int, struct iovec *, int, struct iovec *, int);

#endif
