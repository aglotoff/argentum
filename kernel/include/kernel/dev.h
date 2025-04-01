#ifndef __KERNEL_INCLUDE_KERNEL_DEV_H__
#define __KERNEL_INCLUDE_KERNEL_DEV_H__

#include <sys/types.h>

struct Buf;
struct timeval;
struct Thread;

struct CharDev {
  int     (*open)(struct Thread *, dev_t, int, mode_t);
  ssize_t (*read)(struct Thread *, dev_t, uintptr_t, size_t);
  ssize_t (*write)(struct Thread *, dev_t, uintptr_t, size_t);
  int     (*ioctl)(struct Thread *, dev_t, int, int);
  int     (*select)(struct Thread *, dev_t, struct timeval *);
};

struct BlockDev {
  void    (*request)(struct Buf *);
};

struct CharDev  *dev_lookup_char(dev_t);
void             dev_register_char(int, struct CharDev *);

struct BlockDev *dev_lookup_block(dev_t);
void             dev_register_block(int, struct BlockDev *);

#endif  // !__KERNEL_INCLUDE_KERNEL_DEV_H__
