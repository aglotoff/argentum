#ifndef __KERNEL_BUF_H__
#define __KERNEL_BUF_H__

#include <stdint.h>

#include "list.h"
#include "process.h"
#include "spinlock.h"

#define BLOCK_SIZE  1024

struct Buf {
  int flags;
  unsigned block_no;
  struct ListLink queue_link;
  struct WaitQueue wait_queue;
  struct Spinlock lock;
  uint8_t data[BLOCK_SIZE];
};

#define BUF_VALID   (1 << 0)  ///< Buffer has been read from disk
#define BUF_DIRTY   (1 << 1)  ///< Buffer needs to be written to disk

#endif  // !__KERNEL_BUF_H__
