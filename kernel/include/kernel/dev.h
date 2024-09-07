#ifndef __KERNEL_INCLUDE_KERNEL_DEV_H__
#define __KERNEL_INCLUDE_KERNEL_DEV_H__

#include <sys/types.h>

struct Buf;
struct timeval;

struct CharDev {
  ssize_t (*read)(dev_t, uintptr_t, size_t);
  ssize_t (*write)(dev_t, const void *, size_t);
  int     (*ioctl)(dev_t, int, int);
  int     (*select)(dev_t, struct timeval *);
};

struct BlockDev {
  void    (*request)(struct Buf *);
};

struct CharDev  *dev_lookup_char(dev_t);
void             dev_register_char(int, struct CharDev *);

struct BlockDev *dev_lookup_block(dev_t);
void             dev_register_block(int, struct BlockDev *);

#endif  // !__KERNEL_INCLUDE_KERNEL_DEV_H__
