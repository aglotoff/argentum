#include <assert.h>
#include <errno.h>

#include "console.h"
#include "file.h"
#include "kobject.h"
#include "sync.h"

static struct SpinLock file_lock;
static struct KObjectPool *file_pool;

void
file_init(void)
{
  if (!(file_pool = kobject_pool_create("file_pool", sizeof(struct File), 0)))
    panic("Cannot allocate file pool");

  spin_init(&file_lock, "file_lock");
}

// ssize_t
// file_write(struct File *f, const void *buf, size_t nbytes)
// {
//   int r;

//   if (!f->writeable)
//     return -EBADF;
  
  
// }
