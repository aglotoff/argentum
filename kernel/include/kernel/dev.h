#ifndef __KERNEL_INCLUDE_KERNEL_DEV_H__
#define __KERNEL_INCLUDE_KERNEL_DEV_H__

#include <sys/types.h>

struct BufRequest;
struct timeval;
struct IpcRequest;

struct CharDev {
  int     (*open)(struct IpcRequest *, dev_t, int, mode_t);
  ssize_t (*read)(struct IpcRequest *, dev_t, uintptr_t, size_t);
  ssize_t (*write)(struct IpcRequest *, dev_t, uintptr_t, size_t);
  int     (*ioctl)(struct IpcRequest *, dev_t, int, int);
  int     (*select)(struct IpcRequest *, dev_t, struct timeval *);
};

struct BlockDev {
  void    (*request)(struct BufRequest *);
};

struct CharDev  *dev_lookup_char(dev_t);
void             dev_register_char(int, struct CharDev *);

struct BlockDev *dev_lookup_block(dev_t);
void             dev_register_block(int, struct BlockDev *);

int              dev_open(struct IpcRequest *, dev_t, int, mode_t);
ssize_t          dev_read(struct IpcRequest *, dev_t, uintptr_t, size_t);
ssize_t          dev_write(struct IpcRequest *, dev_t, uintptr_t, size_t);
int              dev_ioctl(struct IpcRequest *, dev_t, int, int);
int              dev_select(struct IpcRequest *, dev_t, struct timeval *);

#endif  // !__KERNEL_INCLUDE_KERNEL_DEV_H__
