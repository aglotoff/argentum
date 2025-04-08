#ifndef __KERNEL_INCLUDE_KERNEL_IPC_CHANNEL_H__
#define __KERNEL_INCLUDE_KERNEL_IPC_CHANNEL_H__

#include <sys/types.h>

struct Inode;
struct stat;
struct Pipe;

enum {
  CHANNEL_TYPE_FILE = 1,
  CHANNEL_TYPE_PIPE = 2,
  CHANNEL_TYPE_SOCKET = 3,
};

struct Channel {
  int                  type;         ///< File type (inode, console, or pipe)
  int                  ref_count;    ///< The number of references to this file
  
  int                  flags;
  
  union {
    int                socket;       ///< Socket ID
    struct Pipe       *pipe;         ///< Pointer to the correspondig pipe
    struct {
      off_t            offset;       ///< Current offset within the file
      struct PathNode *node;        ///< Pointer to the corresponding inode
      struct Inode    *inode;
      dev_t            rdev;
      struct FS       *fs;
    } file;
  } u;
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

#endif  // !__KERNEL_INCLUDE_KERNEL_IPC_CHANNEL__
