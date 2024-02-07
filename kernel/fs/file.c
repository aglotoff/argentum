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

#include "ext2.h"

static struct SpinLock file_lock;
static struct ObjectPool *file_cache;

void
file_init(void)
{
  if (!(file_cache = object_pool_create("file_cache", sizeof(struct File), 0, 0, NULL, NULL)))
    panic("Cannot allocate file cache");

  spin_init(&file_lock, "file_lock");
}

int
file_alloc(struct File **fstore)
{
  struct File *f;

  if ((f = (struct File *) object_pool_get(file_cache)) == NULL)
    return -ENOMEM;

  f->type      = 0;
  f->ref_count = 0;
  f->flags     = 0;
  f->offset    = 0;
  f->inode     = NULL;
  f->socket    = 0;
  f->pipe      = NULL;

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC)

int
file_open(const char *path, int oflag, mode_t mode, struct File **fstore)
{
  struct File *f;
  struct Inode *ip;
  int r;

  // TODO: ENFILE
  if ((r = file_alloc(&f)) != 0)
    return r;

  f->flags = oflag & (STATUS_MASK | O_ACCMODE);
  f->type  = FD_INODE;

  // TODO: the check and the file creation should be atomic
  if ((r = fs_name_lookup(path, 0, &ip)) < 0)
    goto out1;

  if (ip == NULL) {
    if (!(oflag & O_CREAT)) {
      r = -ENOENT;
      goto out1;
    }
    
    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);

    if ((r = fs_create(path, S_IFREG | mode, 0, &ip)) < 0)
      goto out1;
  } else {
    fs_inode_lock(ip);

    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      r = -EEXIST;
      goto out2;
    }
  }

  if (S_ISDIR(ip->mode) && (oflag & O_WRONLY)) {
    r = -ENOTDIR;
    goto out2;
  }

  // TODO: O_NOCTTY
  // TODO: O_NONBLOCK
  // TODO: ENXIO
  // TODO: EROFS

  if ((oflag & O_WRONLY) && (oflag & O_TRUNC)) {
    if ((r = fs_inode_truncate(ip)) < 0)
      goto out2;
  }

  f->inode = ip;

  if (oflag & O_RDONLY) {
    // TODO: check group and other permissions
    if (!(f->inode->mode & S_IRUSR)) {
      r = -EPERM;
      goto out2;
    }
  }

  if (oflag & O_WRONLY) {
    // TODO: check group and other permissions
    if (!(f->inode->mode & S_IWUSR)) {
      r = -EPERM;
      goto out2;
    }
  }

  fs_inode_unlock(ip);

  if (oflag & O_APPEND)
    f->offset = ip->size;

  f->ref_count++;

  *fstore = f;

  return 0;

out2:
  fs_inode_unlock_put(ip);
out1:
  object_pool_put(file_cache, f);
  return r;
}

struct File *
file_dup(struct File *f)
{
  spin_lock(&file_lock);
  f->ref_count++;
  spin_unlock(&file_lock);

  return f;
}

void
file_close(struct File *f)
{
  int ref_count;
  
  spin_lock(&file_lock);

  if (f->ref_count < 1)
    panic("Invalid ref_count");

  ref_count = --f->ref_count;

  spin_unlock(&file_lock);

  if (ref_count > 0)
    return;

  switch (f->type) {
    case FD_INODE:
      assert(f->inode != NULL);
      fs_inode_put(f->inode);
      break;
    case FD_PIPE:
      assert(f->pipe != NULL);
      pipe_close(f->pipe, (f->flags & O_ACCMODE) != O_RDONLY);
      break;
    case FD_SOCKET:
      net_close(f->socket);
      break;
    default:
      panic("Invalid type");
  }

  object_pool_put(file_cache, f);
}

off_t
file_seek(struct File *f, off_t offset, int whence)
{
  // TODO: validate
  off_t new_offset;

  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);

    switch (whence) {
    case SEEK_SET:
      new_offset = offset;
      break;
    case SEEK_CUR:
      new_offset = f->offset + offset;
      break;
    case SEEK_END:
      fs_inode_lock(f->inode);
      new_offset = f->inode->size + offset;
      fs_inode_unlock(f->inode);
      break;
    default:
      return -EINVAL;
    }

    if (new_offset < 0)
      return -EOVERFLOW;

    f->offset = new_offset;

    return new_offset;

  case FD_PIPE:
  case FD_SOCKET:
    return -ESPIPE;

  default:
    panic("Invalid type");
    return 0;
  }
}

ssize_t
file_read(struct File *f, void *buf, size_t nbytes)
{
  int r;

  if ((f->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  
  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);

    fs_inode_lock(f->inode);

    // TODO: check group and other permissions
    // TODO: what if permissions change?
    if (!(f->inode->mode & S_IRUSR)) {
      fs_inode_unlock(f->inode);
      return -EPERM;
    }

    r = fs_inode_read(f->inode, buf, nbytes, &f->offset);

    fs_inode_unlock(f->inode);

    return r;

  case FD_SOCKET:
    return net_recvfrom(f->socket, buf, nbytes, 0, NULL, NULL);

  case FD_PIPE:
    assert(f->pipe != NULL);
    return pipe_read(f->pipe, buf, nbytes);

  default:
    panic("Invalid type");
    return 0;
  }
}

ssize_t
file_getdents(struct File *f, void *buf, size_t nbytes)
{
  int r;

  if ((f->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  if ((f->type != FD_INODE) || !S_ISDIR(f->inode->mode))
    return -ENOTDIR;

  fs_inode_lock(f->inode);

  // TODO: check group and other permissions
  // TODO: what if permissions change?
  if (!(f->inode->mode & S_IRUSR)) {
    fs_inode_unlock(f->inode);
    return -EPERM;
  }

  r = fs_inode_read_dir(f->inode, buf, nbytes, &f->offset);

  fs_inode_unlock(f->inode);

  return r;
}

ssize_t
file_write(struct File *f, const void *buf, size_t nbytes)
{
  int r;

  if ((f->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);

    fs_inode_lock(f->inode);

    // TODO: check group and other permissions
    // TODO: what if permissions change?
    if (!(f->inode->mode & S_IWUSR)) {
      fs_inode_unlock(f->inode);
      return -EPERM;
    }

    if (f->flags & O_APPEND)
      f->offset = f->inode->size;

    r = fs_inode_write(f->inode, buf, nbytes, &f->offset);

    fs_inode_unlock(f->inode);

    return r;
  
  case FD_SOCKET:
    return net_sendto(f->socket, buf, nbytes, 0, NULL, 0);

  case FD_PIPE:
    assert(f->pipe != NULL);
    return pipe_write(f->pipe, buf, nbytes);

  default:
    panic("Invalid type");
    return 0;
  }
}

int
file_stat(struct File *fp, struct stat *buf)
{
  int r;
  
  if (fp->type != FD_INODE) 
    return -EBADF;

  assert(fp->inode != NULL);

  fs_inode_lock(fp->inode);
  r = fs_inode_stat(fp->inode, buf);
  fs_inode_unlock(fp->inode);

  return r;
}

int
file_chdir(struct File *file)
{
  int r;

  if (file->type != FD_INODE)
    return -ENOTDIR;

  fs_inode_lock(file->inode);
  r = fs_set_pwd(file->inode);
  fs_inode_unlock(file->inode);

  return r;
}

int
file_get_flags(struct File *file)
{
  return file->flags & STATUS_MASK;
}

int
file_set_flags(struct File *file, int flags)
{
  file->flags = (file->flags & ~STATUS_MASK) | (flags & STATUS_MASK);
  return 0;
}

int
file_chmod(struct File *file, mode_t mode)
{
  int r;
  
  if (file->type != FD_INODE) 
    return -EINVAL;

  assert(file->inode != NULL);

  fs_inode_lock(file->inode);
  r = fs_inode_chmod(file->inode, mode);
  fs_inode_unlock(file->inode);

  return r;
}

int
file_ioctl(struct File *f, int request, int arg)
{
  int r;

  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);

    fs_inode_lock(f->inode);
    r = fs_inode_ioctl(f->inode, request, arg);
    fs_inode_unlock(f->inode);

    return r;

  default:
    panic("ertrtrtr %d\n", f->type);
    return -EINVAL;
  }
}
