#include <kernel/assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/fs/file.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/spinlock.h>
#include <kernel/net.h>
#include <kernel/pipe.h>

static struct KSpinLock file_lock;
static struct KObjectPool *file_cache;

void
file_init(void)
{
  if (!(file_cache = k_object_pool_create("file_cache", sizeof(struct File), 0, NULL, NULL)))
    panic("Cannot allocate file cache");

  k_spinlock_init(&file_lock, "file_lock");
}

int
file_alloc(struct File **fstore)
{
  struct File *f;

  if ((f = (struct File *) k_object_pool_get(file_cache)) == NULL)
    return -ENOMEM;

  f->type      = 0;
  f->ref_count = 0;
  f->flags     = 0;
  f->offset    = 0;
  f->node      = NULL;
  f->socket    = 0;
  f->pipe      = NULL;

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

struct File *
file_dup(struct File *file)
{
  k_spinlock_acquire(&file_lock);
  file->ref_count++;
  k_spinlock_release(&file_lock);

  return file;
}

#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC)

int
file_get_flags(struct File *file)
{
  int r;

  k_spinlock_acquire(&file_lock);
  r = file->flags & STATUS_MASK;
  k_spinlock_release(&file_lock);

  return r;
}

int
file_set_flags(struct File *file, int flags)
{
  k_spinlock_acquire(&file_lock);
  file->flags = (file->flags & ~STATUS_MASK) | (flags & STATUS_MASK);
  k_spinlock_release(&file_lock);
  return 0;
}

void
file_put(struct File *file)
{
  int ref_count;
  
  k_spinlock_acquire(&file_lock);

  if (file->ref_count < 1)
    panic("bad ref_count %d", ref_count);

  ref_count = --file->ref_count;

  k_spinlock_release(&file_lock);

  if (ref_count > 0)
    return;

  switch (file->type) {
    case FD_INODE:
      fs_close(file);
      break;
    case FD_PIPE:
      pipe_close(file);
      break;
    case FD_SOCKET:
      net_close(file);
      break;
    default:
      panic("bad file type");
  }

  k_object_pool_put(file_cache, file);
}

off_t
file_seek(struct File *file, off_t offset, int whence)
{
  if ((whence != SEEK_SET) && (whence != SEEK_CUR) && (whence != SEEK_SET))
    return -EINVAL;

  switch (file->type) {
  case FD_INODE:
    return fs_seek(file, offset, whence);
  case FD_PIPE:
  case FD_SOCKET:
    return -ESPIPE;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

ssize_t
file_read(struct File *file, uintptr_t va, size_t nbytes)
{
  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  
  switch (file->type) {
  case FD_INODE:
    return fs_read(file, va, nbytes);
  case FD_SOCKET:
    return net_read(file, va, nbytes);
  case FD_PIPE:
    return pipe_read(file, va, nbytes);
  default:
    panic("bad file type");
    return -EBADF;
  }
}

ssize_t
file_write(struct File *file, uintptr_t va, size_t nbytes)
{
  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  switch (file->type) {
  case FD_INODE:
    return fs_write(file, va, nbytes);
  case FD_SOCKET:
    return net_write(file, va, nbytes);
  case FD_PIPE:
    return pipe_write(file, va, nbytes);
  default:
    panic("bad file type");
    return -EBADF;
  }
}

ssize_t
file_getdents(struct File *file, uintptr_t va, size_t nbytes)
{
  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  switch (file->type) {
  case FD_INODE:
    return fs_getdents(file, va, nbytes);
  case FD_SOCKET:
  case FD_PIPE:
    return -ENOTDIR;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_stat(struct File *file, struct stat *buf)
{
  switch (file->type) {
  case FD_INODE:
    return fs_fstat(file, buf);
  case FD_PIPE:
    return pipe_stat(file, buf);
  case FD_SOCKET:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_chdir(struct File *file)
{
  switch (file->type) {
  case FD_INODE:
    return fs_fchdir(file);
  case FD_SOCKET:
  case FD_PIPE:
    return -ENOTDIR;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_chmod(struct File *file, mode_t mode)
{
  switch (file->type) {
  case FD_INODE:
    return fs_fchmod(file, mode);
  case FD_SOCKET:
  case FD_PIPE:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_chown(struct File *file, uid_t uid, gid_t gid)
{
  switch (file->type) {
  case FD_INODE:
    return fs_fchown(file, uid, gid);
  case FD_SOCKET:
  case FD_PIPE:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_ioctl(struct File *file, int request, int arg)
{
  switch (file->type) {
  case FD_INODE:
    return fs_ioctl(file, request, arg);
  case FD_SOCKET:
  case FD_PIPE:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_select(struct File *file, struct timeval *timeout)
{
  switch (file->type) {
  case FD_INODE:
    return fs_select(file, timeout);
  case FD_SOCKET:
  case FD_PIPE:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_truncate(struct File *file, off_t length)
{
  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  switch (file->type) {
  case FD_INODE:
    return fs_ftruncate(file, length);
  case FD_SOCKET:
  case FD_PIPE:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}

int
file_sync(struct File *file)
{
  switch (file->type) {
  case FD_INODE:
    return fs_fsync(file);
  case FD_SOCKET:
  case FD_PIPE:
    return -EBADF;
  default:
    panic("bad file type");
    return -EBADF;
  }
}
