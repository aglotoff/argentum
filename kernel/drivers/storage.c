#include <kernel/assert.h>
#include <kernel/fs/buf.h>
#include <kernel/storage.h>
#include <kernel/mach.h>

int
storage_init(void)
{
  return mach_current->storage_init();
}

/**
 * Add buffer to the request queue and put the current process to sleep until
 * the operation is completed.
 * 
 * @param buf The buffer to be processed.
 */
void
storage_request(struct Buf *buf)
{
  if (!k_mutex_holding(&buf->mutex))
    panic("buf not locked");
  if ((buf->flags & (BUF_DIRTY | BUF_VALID)) == BUF_VALID)
    panic("nothing to do");
  if (buf->dev != 0)
    panic("dev must be 0, %d given", buf->dev);

  mach_current->storage_request(buf);
}
