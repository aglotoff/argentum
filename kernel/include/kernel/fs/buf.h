#ifndef __KERNEL_INCLUDE_KERNEL_FS_BUF_H__
#define __KERNEL_INCLUDE_KERNEL_FS_BUF_H__

#include <stdint.h>
#include <sys/types.h>

#include <kernel/core/list.h>
#include <kernel/core/mutex.h>
#include <kernel/core/condvar.h>

struct Buf {
  unsigned long    block_no;      // Filesystem block number
  dev_t            dev;           // ID of the device this block belongs to
  size_t           block_size;    // Must be BLOCK_SIZE
  uint8_t         *data;          // Block data

  struct KMutex    _mutex;        // Mutex protecting the block data
  int              _flags;        // Status flags
  int              _ref_count;    // The number of references to the block
  struct KListLink _cache_link;   // Link into the buf cache
};

void        buf_init(void);
struct Buf *buf_read(unsigned, size_t, dev_t);
void        buf_write(struct Buf *);
void        buf_release(struct Buf *);

struct BufRequest {
  struct Buf      *buf;
  int              type;
  struct KListLink queue_link;    // Link into the driver queue

  struct KCondVar  _wait_cond;     // Processes waiting for the block data
};

enum {
  BUF_REQUEST_READ  = 0,
  BUF_REQUEST_WRITE = 1,
};

static inline int
buf_request_wait(struct BufRequest *req, struct KMutex *mutex)
{
  return k_condvar_wait(&req->_wait_cond, mutex, 0);
}

static inline int
buf_request_wakeup(struct BufRequest *req)
{
  return k_condvar_notify_all(&req->_wait_cond);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_FS_BUF_H__
