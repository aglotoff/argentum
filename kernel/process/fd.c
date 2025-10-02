#include <errno.h>
#include <fcntl.h>

#include <kernel/core/assert.h>
#include <kernel/console.h>
#include <kernel/process.h>
#include <kernel/fd.h>
#include <kernel/ipc.h>

static struct ConnectionDesc *_fd_lookup(struct Process *, int);
static void                _fd_close(struct ConnectionDesc *);

void
fd_init(struct Process *process)
{
  int i;

  for (i = 0; i < OPEN_MAX; i++) {
    process->connections[i].connection = NULL;
    process->connections[i].flags = 0;
  }

  k_spinlock_init(&process->connections_lock, "connections_lock");
}

void
fd_close_all(struct Process *process)
{
  int i;

  // TODO: no need to lock?

  for (i = 0; i < OPEN_MAX; i++)
    if (process->connections[i].connection != NULL)
      _fd_close(&process->connections[i]);
}

void
fd_close_on_exec(struct Process *process)
{
  int i;

  // TODO: no need to lock the parent?

  for (i = 0; i < OPEN_MAX; i++)
    if (process->connections[i].flags & FD_CLOEXEC) 
      _fd_close(&process->connections[i]);
}

void
fd_clone(struct Process *parent, struct Process *child)
{
  int i;

  // TODO: no need to lock the parent?

  k_spinlock_acquire(&child->connections_lock);

  for (i = 0; i < OPEN_MAX; i++) {
    if (parent->connections[i].connection != NULL){
      child->connections[i].connection  = connection_ref(parent->connections[i].connection);
      child->connections[i].flags = parent->connections[i].flags;
    }
  }

  k_spinlock_release(&child->connections_lock);
}

int
fd_alloc(struct Process *process, struct Connection *f, int start)
{
  int i;

  if (start < 0 || start >= OPEN_MAX)
    return -EINVAL;

  k_spinlock_acquire(&process->connections_lock);

  for (i = start; i < OPEN_MAX; i++) {
    if (process->connections[i].connection == NULL) {
      process->connections[i].connection = connection_ref(f);
      process->connections[i].flags = 0;

      k_spinlock_release(&process->connections_lock);

      return i;
    }
  }

  k_spinlock_release(&process->connections_lock);

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
struct Connection *
fd_lookup(struct Process *process, int n)
{
  struct ConnectionDesc *fd;

  k_spinlock_acquire(&process->connections_lock);
  fd = _fd_lookup(process, n);
  k_spinlock_release(&process->connections_lock);

  return (fd == NULL || fd->connection == NULL) ?  NULL : connection_ref(fd->connection);
}

int
fd_close(struct Process *process, int n)
{
  struct ConnectionDesc *fd;
  struct Connection *connection;

  k_spinlock_acquire(&process->connections_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->connection == NULL)) {
    k_spinlock_release(&process->connections_lock);
    return -EBADF;
  }

  connection = fd->connection;

  fd->connection  = NULL;
  fd->flags = 0;

  k_spinlock_release(&process->connections_lock);

  connection_unref(connection);

  return 0;
}

int
fd_get_flags(struct Process *process, int n)
{
  struct ConnectionDesc *fd;
  int flags = 0;

  k_spinlock_acquire(&process->connections_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->connection == NULL)) {
    k_spinlock_release(&process->connections_lock);
    return -EBADF;
  }

  flags = fd->flags;

  k_spinlock_release(&process->connections_lock);

  return flags;
}

int
fd_set_flags(struct Process *process, int n, int flags)
{
  struct ConnectionDesc *fd;

  if (flags & ~FD_CLOEXEC)
    return -EINVAL;

  k_spinlock_acquire(&process->connections_lock);

  fd = _fd_lookup(process, n);

  if ((fd == NULL) || (fd->connection == NULL)) {
    k_spinlock_release(&process->connections_lock);
    return -EBADF;
  }

  fd->flags = flags;

  k_spinlock_release(&process->connections_lock);

  return 0;
}

static struct ConnectionDesc *
_fd_lookup(struct Process *process, int n)
{
  if ((n < 0) || (n >= OPEN_MAX))
    return NULL;
  return &process->connections[n];
}

static void
_fd_close(struct ConnectionDesc *fd)
{
  k_assert(fd->connection != NULL);
  
  connection_unref(fd->connection);

  fd->connection  = NULL;
  fd->flags = 0;
}
