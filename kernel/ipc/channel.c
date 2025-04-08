#include <kernel/core/assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/console.h>
#include <kernel/ipc/channel.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/core/spinlock.h>
#include <kernel/net.h>
#include <kernel/pipe.h>

static struct KSpinLock channel_lock;
static struct KObjectPool *channel_pool;

void
channel_init(void)
{
  if (!(channel_pool = k_object_pool_create("channel_pool", sizeof(struct Channel), 0, NULL, NULL)))
    k_panic("Cannot allocate channel pool");

  k_spinlock_init(&channel_lock, "channel_lock");
}

int
channel_alloc(struct Channel **fstore)
{
  struct Channel *f;

  if ((f = (struct Channel *) k_object_pool_get(channel_pool)) == NULL)
    return -ENOMEM;

  f->type      = 0;
  f->ref_count = 0;
  f->flags     = 0;
  f->u.file.offset    = 0;
  f->u.file.node      = NULL;
  f->u.file.inode     = NULL;
  f->u.file.rdev      = -1;
  f->u.file.fs   = NULL;
  f->u.socket    = 0;
  f->u.pipe      = NULL;

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

struct Channel *
channel_ref(struct Channel *channel)
{
  k_spinlock_acquire(&channel_lock);
  channel->ref_count++;
  k_spinlock_release(&channel_lock);

  return channel;
}

// FIXME: this is a random guess
#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC | O_RDONLY | O_RDWR | O_WRONLY)

int
channel_get_flags(struct Channel *channel)
{
  int r;

  k_spinlock_acquire(&channel_lock);
  r = channel->flags & STATUS_MASK;
  k_spinlock_release(&channel_lock);

  return r;
}

int
channel_set_flags(struct Channel *channel, int flags)
{
  k_spinlock_acquire(&channel_lock);
  channel->flags = (channel->flags & ~STATUS_MASK) | (flags & STATUS_MASK);
  k_spinlock_release(&channel_lock);
  return 0;
}

void
channel_unref(struct Channel *channel)
{
  int ref_count;
  
  k_spinlock_acquire(&channel_lock);

  if (channel->ref_count < 1)
    k_panic("bad ref_count %d", ref_count);

  ref_count = --channel->ref_count;

  k_spinlock_release(&channel_lock);

  if (ref_count > 0)
    return;

  switch (channel->type) {
    case CHANNEL_TYPE_FILE:
      fs_close(channel);
      break;
    case CHANNEL_TYPE_PIPE:
      pipe_close(channel);
      break;
    case CHANNEL_TYPE_SOCKET:
      net_close(channel);
      break;
    default:
      k_panic("bad channel type %d", channel->type);
  }

  k_object_pool_put(channel_pool, channel);
}

off_t
channel_seek(struct Channel *channel, off_t offset, int whence)
{
  if ((whence != SEEK_SET) && (whence != SEEK_CUR) && (whence != SEEK_END))
    return -EINVAL;

  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_seek(channel, offset, whence);
  case CHANNEL_TYPE_PIPE:
  case CHANNEL_TYPE_SOCKET:
    return -ESPIPE;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

ssize_t
channel_read(struct Channel *channel, uintptr_t va, size_t nbytes)
{
  if ((channel->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_read(channel, va, nbytes);
  case CHANNEL_TYPE_SOCKET:
    return net_read(channel, va, nbytes);
  case CHANNEL_TYPE_PIPE:
    return pipe_read(channel, va, nbytes);
  default:
  k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

ssize_t
channel_write(struct Channel *channel, uintptr_t va, size_t nbytes)
{
  if ((channel->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_write(channel, va, nbytes);
  case CHANNEL_TYPE_SOCKET:
    return net_write(channel, va, nbytes);
  case CHANNEL_TYPE_PIPE:
    return pipe_write(channel, va, nbytes);
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

ssize_t
channel_getdents(struct Channel *channel, uintptr_t va, size_t nbytes)
{
  if ((channel->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_getdents(channel, va, nbytes);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -ENOTDIR;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_stat(struct Channel *channel, struct stat *buf)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_fstat(channel, buf);
  case CHANNEL_TYPE_PIPE:
    return pipe_stat(channel, buf);
  case CHANNEL_TYPE_SOCKET:
    return -EBADF;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_chdir(struct Channel *channel)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_fchdir(channel);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -ENOTDIR;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_chmod(struct Channel *channel, mode_t mode)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_fchmod(channel, mode);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -EBADF;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_chown(struct Channel *channel, uid_t uid, gid_t gid)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_fchown(channel, uid, gid);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -EBADF;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_ioctl(struct Channel *channel, int request, int arg)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_ioctl(channel, request, arg);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -EBADF;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_select(struct Channel *channel, struct timeval *timeout)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_select(channel, timeout);
  case CHANNEL_TYPE_SOCKET:
    return net_select(channel, timeout);
  case CHANNEL_TYPE_PIPE:
    return pipe_select(channel, timeout);
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_truncate(struct Channel *channel, off_t length)
{
  if ((channel->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_ftruncate(channel, length);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -EBADF;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_sync(struct Channel *channel)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_fsync(channel);
  case CHANNEL_TYPE_SOCKET:
  case CHANNEL_TYPE_PIPE:
    return -EBADF;
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}
