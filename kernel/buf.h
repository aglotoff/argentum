#ifndef __KERNEL_BUF_H__
#define __KERNEL_BUF_H__

#include <stdint.h>

#include "list.h"
#include "process.h"
#include "sleeplock.h"

#define BUF_CACHE_SIZE  32
#define BLOCK_SIZE      1024

struct Buf {
  int flags;
  int ref_count;
  unsigned block_no;
  struct ListLink cache_link;
  struct ListLink queue_link;
  struct Sleeplock lock;
  struct WaitQueue wait_queue;
  uint8_t data[BLOCK_SIZE];
};

#define BUF_VALID   (1 << 0)  ///< Buffer has been read from disk
#define BUF_DIRTY   (1 << 1)  ///< Buffer needs to be written to disk

void        buf_init(void);
struct Buf *buf_read(unsigned);
void        buf_write(struct Buf *);
void        buf_release(struct Buf *);

#endif  // !__KERNEL_BUF_H__
