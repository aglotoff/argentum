#include <errno.h>
#include <fcntl.h>

#include <kernel/assert.h>
#include <kernel/cprintf.h>
#include <kernel/process.h>
#include <kernel/fd.h>
#include <kernel/fs/file.h>

static struct FileDesc *_fd_lookup(struct Process *, int);
static void             _fd_close(struct FileDesc *);

void
fd_init(struct Process *process)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++) {
    process->fd[i].file  = NULL;
    process->fd[i].flags = 0;
  }

  k_spinlock_init(&process->fd_lock, "fd_lock");
}

void
fd_close_all(struct Process *process)
{
  int i;

  // TODO: no need to lock?

  for (i = 0; i < OPEN_MAX; i++)
    if (process->fd[i].file != NULL)
      _fd_close(&process->fd[i]);
}

void
fd_close_on_exec(struct Process *process)
{
  int i;

  // TODO: no need to lock the parent?

  for (i = 0; i < OPEN_MAX; i++)
    if (process->fd[i].flags & FD_CLOEXEC) 
      _fd_close(&process->fd[i]);
}

void
fd_clone(struct Process *parent, struct Process *child)
{
  int i;

  // TODO: no need to lock the parent?

  k_spinlock_acquire(&child->fd_lock);

  for (i = 0; i < OPEN_MAX; i++) {
    if (parent->fd[i].file != NULL){
      child->fd[i].file  = file_dup(parent->fd[i].file);
      child->fd[i].flags = parent->fd[i].flags;
    }
  }

  k_spinlock_release(&child->fd_lock);
}

int
fd_alloc(struct Process *process, struct File *f, int start)
{
  int i;

  if (start < 0 || start >= OPEN_MAX)
    return -EINVAL;

  k_spinlock_acquire(&process->fd_lock);

  for (i = start; i < OPEN_MAX; i++) {
    if (process->fd[i].file == NULL) {
      process->fd[i].file  = f;
      process->fd[i].flags = 0;

      k_spinlock_release(&process->fd_lock);

      return i;
    }
  }

  k_spinlock_release(&process->fd_lock);

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
struct File *
fd_lookup(struct Process *process, int n)
{
  struct FileDesc *fd;

  k_spinlock_acquire(&process->fd_lock);
  fd = _fd_lookup(process, n);
  k_spinlock_release(&process->fd_lock);

  return (fd == NULL || fd->file == NULL) ?  NULL : file_dup(fd->file);
}

int
fd_close(struct Process *process, int n)
{
  struct FileDesc *fd;
  struct File *file;

  k_spinlock_acquire(&process->fd_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->file == NULL)) {
    k_spinlock_release(&process->fd_lock);
    return -EBADF;
  }

  file = fd->file;

  fd->file  = NULL;
  fd->flags = 0;

  k_spinlock_release(&process->fd_lock);

  // cprintf("     file_put %d %d\n", n, file->ref_count);

  file_put(file);

  return 0;
}

int
fd_get_flags(struct Process *process, int n)
{
  struct FileDesc *fd;
  int flags = 0;

  k_spinlock_acquire(&process->fd_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->file == NULL)) {
    k_spinlock_release(&process->fd_lock);
    return -EBADF;
  }

  flags = fd->flags;

  k_spinlock_release(&process->fd_lock);

  return flags;
}

int
fd_set_flags(struct Process *process, int n, int flags)
{
  struct FileDesc *fd;

  if (flags & ~FD_CLOEXEC)
    return -EINVAL;

  k_spinlock_acquire(&process->fd_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->file == NULL)) {
    k_spinlock_release(&process->fd_lock);
    return -EBADF;
  }

  fd->flags = flags;

  k_spinlock_release(&process->fd_lock);

  return 0;
}

static struct FileDesc *
_fd_lookup(struct Process *process, int n)
{
  if ((n < 0) || (n >= OPEN_MAX))
    return NULL;

  return &process->fd[n];
}

static void
_fd_close(struct FileDesc *fd)
{
  assert(fd->file != NULL);
  
  file_put(fd->file);

  fd->file  = NULL;
  fd->flags = 0;
}

