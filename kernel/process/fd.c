#include <errno.h>
#include <fcntl.h>

#include <kernel/core/assert.h>
#include <kernel/console.h>
#include <kernel/process.h>
#include <kernel/fd.h>
#include <kernel/ipc/channel.h>

static struct ChannelDesc *_fd_lookup(struct Process *, int);
static void                _fd_close(struct ChannelDesc *);

void
fd_init(struct Process *process)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++) {
    process->channels[i].channel = NULL;
    process->channels[i].flags = 0;
  }

  k_spinlock_init(&process->channels_lock, "channels_lock");
}

void
fd_close_all(struct Process *process)
{
  int i;

  // TODO: no need to lock?

  for (i = 0; i < OPEN_MAX; i++)
    if (process->channels[i].channel != NULL)
      _fd_close(&process->channels[i]);
}

void
fd_close_on_exec(struct Process *process)
{
  int i;

  // TODO: no need to lock the parent?

  for (i = 0; i < OPEN_MAX; i++)
    if (process->channels[i].flags & FD_CLOEXEC) 
      _fd_close(&process->channels[i]);
}

void
fd_clone(struct Process *parent, struct Process *child)
{
  int i;

  // TODO: no need to lock the parent?

  k_spinlock_acquire(&child->channels_lock);

  for (i = 0; i < OPEN_MAX; i++) {
    if (parent->channels[i].channel != NULL){
      child->channels[i].channel  = channel_ref(parent->channels[i].channel);
      child->channels[i].flags = parent->channels[i].flags;
    }
  }

  k_spinlock_release(&child->channels_lock);
}

int
fd_alloc(struct Process *process, struct Channel *f, int start)
{
  int i;

  if (start < 0 || start >= OPEN_MAX)
    return -EINVAL;

  k_spinlock_acquire(&process->channels_lock);

  for (i = start; i < OPEN_MAX; i++) {
    if (process->channels[i].channel == NULL) {
      process->channels[i].channel = channel_ref(f);
      process->channels[i].flags = 0;

      k_spinlock_release(&process->channels_lock);

      return i;
    }
  }

  k_spinlock_release(&process->channels_lock);

  return -EMFILE;
}

/**
 * Fetch the nth system call argument as a file descriptor. Check that the
 * file desriptor is valid.
 * 
 * @param n       The argument number.
 * @param fdstore Pointer to the memory address to store the file descriptor.
 * @param fstore  Pointer to the memory address to store the file.
 * 
 * @retval 0 on success.
 * @retval -EFAULT if the arguments doesn't point to a valid string.
 * @retval -EBADF if the file descriptor is invalid.
 */
struct Channel *
fd_lookup(struct Process *process, int n)
{
  struct ChannelDesc *fd;

  k_spinlock_acquire(&process->channels_lock);
  fd = _fd_lookup(process, n);
  k_spinlock_release(&process->channels_lock);

  return (fd == NULL || fd->channel == NULL) ?  NULL : channel_ref(fd->channel);
}

int
fd_close(struct Process *process, int n)
{
  struct ChannelDesc *fd;
  struct Channel *channel;

  k_spinlock_acquire(&process->channels_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->channel == NULL)) {
    k_spinlock_release(&process->channels_lock);
    return -EBADF;
  }

  channel = fd->channel;

  fd->channel  = NULL;
  fd->flags = 0;

  k_spinlock_release(&process->channels_lock);

  channel_unref(channel);

  return 0;
}

int
fd_get_flags(struct Process *process, int n)
{
  struct ChannelDesc *fd;
  int flags = 0;

  k_spinlock_acquire(&process->channels_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->channel == NULL)) {
    k_spinlock_release(&process->channels_lock);
    return -EBADF;
  }

  flags = fd->flags;

  k_spinlock_release(&process->channels_lock);

  return flags;
}

int
fd_set_flags(struct Process *process, int n, int flags)
{
  struct ChannelDesc *fd;

  if (flags & ~FD_CLOEXEC)
    return -EINVAL;

  k_spinlock_acquire(&process->channels_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->channel == NULL)) {
    k_spinlock_release(&process->channels_lock);
    return -EBADF;
  }

  fd->flags = flags;

  k_spinlock_release(&process->channels_lock);

  return 0;
}

static struct ChannelDesc *
_fd_lookup(struct Process *process, int n)
{
  if ((n < 0) || (n >= OPEN_MAX))
    return NULL;
  return &process->channels[n];
}

static void
_fd_close(struct ChannelDesc *fd)
{
  k_assert(fd->channel != NULL);
  
  channel_unref(fd->channel);

  fd->channel  = NULL;
  fd->flags = 0;
}
