#include <errno.h>
#include <fcntl.h>

#include <kernel/assert.h>
#include <kernel/cprintf.h>
#include <kernel/process.h>
#include <kernel/fd.h>
#include <kernel/fs/file.h>

static struct FileDesc *_fd_lookup(struct Process*, int);
static void             _fd_close(struct FileDesc *);

void
fd_init(struct Process *process)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++) {
    process->fd[i].file  = NULL;
    process->fd[i].flags = 0;
  }
}

void
fd_close_all(struct Process *process)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++)
    if (process->fd[i].file != NULL) {
      _fd_close(&process->fd[i]);
    }
}

void
fd_close_on_exec(struct Process *process)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++)
    if (process->fd[i].flags & FD_CLOEXEC)  {
      _fd_close(&process->fd[i]);
    }
}

void
fd_clone(struct Process *parent, struct Process *child)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++) {
    if (parent->fd[i].file != NULL){
      child->fd[i].file  = file_dup(parent->fd[i].file);
      child->fd[i].flags = parent->fd[i].flags;
    }
  }
}

int
fd_alloc(struct Process *process, struct File *f, int start)
{
  int i;

  if (start < 0 || start >= OPEN_MAX)
    return -EINVAL;

  for (i = start; i < OPEN_MAX; i++) {
    if (process->fd[i].file == NULL) {
      process->fd[i].file  = f;
      process->fd[i].flags = 0;

      // cprintf("fd_alloc(%d): %d\n", process->pid, i);

      return i;
    }
  }

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

  if ((fd = _fd_lookup(process, n)) == NULL)
    return NULL;

  return fd->file;
}

int
fd_close(struct Process *process, int n)
{
  struct FileDesc *fd;

  if ((fd = _fd_lookup(process, n)) == NULL)
    return -EBADF;

  if (fd->file == NULL)
    return -EBADF;

  // cprintf("fd_close(%d): %d\n", process->pid, n);

  _fd_close(fd);

  return 0;
}

int
fd_get_flags(struct Process *process, int n)
{
  struct FileDesc *fd;

  if ((fd = _fd_lookup(process, n)) == NULL)
    return -EBADF;
  if (fd->file == NULL)
    return -EBADF;

  return fd->flags;
}

int
fd_set_flags(struct Process *process, int n, int flags)
{
  struct FileDesc *fd;

  if (flags & ~FD_CLOEXEC)
    return -EINVAL;

  if ((fd = _fd_lookup(process, n)) == NULL)
    return -EBADF;
  if (fd->file == NULL)
    return -EBADF;

  fd->flags = flags;

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
  
  file_close(fd->file);

  fd->file  = NULL;
  fd->flags = 0;
}

