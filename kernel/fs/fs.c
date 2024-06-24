#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#include <kernel/fs/fs.h>
#include <kernel/fs/file.h>
#include <kernel/cprintf.h>

#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC)

int
fs_open(const char *path, int oflag, mode_t mode, struct File **fstore)
{
  struct File *f;
  struct PathNode *pp;
  int r;

  // TODO: ENFILE
  if ((r = file_alloc(&f)) != 0)
    return r;

  f->flags = oflag & (STATUS_MASK | O_ACCMODE);
  f->type  = FD_INODE;
  f->ref_count++;
  f->node = NULL;

  // TODO: the check and the file creation should be atomic
  if ((r = fs_lookup(path, 0, &pp)) < 0)
    goto out1;

  if (pp == NULL) {
    if (!(oflag & O_CREAT)) {
      r = -ENOENT;
      goto out1;
    }
    
    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);

    if ((r = fs_create(path, S_IFREG | mode, 0, &pp)) < 0)
      goto out1;
  } else {
    fs_inode_lock(pp->inode);

    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      r = -EEXIST;
      goto out2;
    }
  }

  if (S_ISDIR(pp->inode->mode) && (oflag & O_WRONLY)) {
    r = -ENOTDIR;
    goto out2;
  }

  // TODO: O_NOCTTY
  // TODO: O_NONBLOCK
  // TODO: ENXIO
  // TODO: EROFS

  if ((oflag & O_WRONLY) && (oflag & O_TRUNC)) {
    if ((r = fs_inode_truncate(pp->inode, 0)) < 0)
      goto out2;
  }

  f->node = pp;

  if (oflag & O_RDONLY) {
    // TODO: check group and other permissions
    if (!(f->node->inode->mode & S_IRUSR)) {
      r = -EPERM;
      goto out2;
    }
  }

  if (oflag & O_WRONLY) {
    // TODO: check group and other permissions
    if (!(f->node->inode->mode & S_IWUSR)) {
      r = -EPERM;
      goto out2;
    }
  }

  if (oflag & O_APPEND)
    f->offset = pp->inode->size;

  fs_inode_unlock(pp->inode);

  *fstore = f;

  return 0;

out2:
  fs_inode_unlock(pp->inode);
  fs_path_put(pp);
out1:
  file_put(f);
  return r;
}

int
fs_close(struct File *file)
{
  if (file->type != FD_INODE)
    return -EBADF;
  
  if (file->node != NULL)
    fs_path_put(file->node);

  return 0;
}

off_t
fs_seek(struct File *file, off_t offset, int whence)
{
  off_t new_offset;

  if (file->type != FD_INODE)
    return -EBADF;

  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = file->offset + offset;
    break;
  case SEEK_END:
    fs_inode_lock(file->node->inode);
    new_offset = file->node->inode->size + offset;
    fs_inode_unlock(file->node->inode);
    break;
  default:
    return -EINVAL;
  }

  if (new_offset < 0)
    return -EOVERFLOW;

  file->offset = new_offset;

  return new_offset;
}

ssize_t
fs_read(struct File *file, void *buf, size_t nbytes)
{
  ssize_t r;

  if (file->type != FD_INODE)
    return -EBADF;

  fs_inode_lock(file->node->inode);

  // TODO: check group and other permissions
  // TODO: what if permissions change?

  if (!(file->node->inode->mode & S_IRUSR)) {
    fs_inode_unlock(file->node->inode);
    return -EPERM;
  }

  r = fs_inode_read(file->node->inode, buf, nbytes, &file->offset);

  fs_inode_unlock(file->node->inode);

  return r;
}

ssize_t
fs_write(struct File *file, const void *buf, size_t nbytes)
{
  ssize_t r;

  if (file->type != FD_INODE)
    return -EBADF;

  fs_inode_lock(file->node->inode);

  // TODO: check group and other permissions
  // TODO: what if permissions change?
  if (!(file->node->inode->mode & S_IWUSR)) {
    fs_inode_unlock(file->node->inode);
    return -EPERM;
  }

  if (file->flags & O_APPEND)
    file->offset = file->node->inode->size;

  r = fs_inode_write(file->node->inode, buf, nbytes, &file->offset);

  fs_inode_unlock(file->node->inode);

  return r;
}

ssize_t
fs_getdents(struct File *file, void *buf, size_t nbytes)
{
  ssize_t r;

  if (file->type != FD_INODE)
    return -EBADF;

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  fs_inode_lock(file->node->inode);

  if (!S_ISDIR(file->node->inode->mode)) {
    fs_inode_unlock(file->node->inode);
    return -ENOTDIR;
  }

  // TODO: check group and other permissions
  // TODO: what if permissions change?

  if (!(file->node->inode->mode & S_IRUSR)) {
    fs_inode_unlock(file->node->inode);
    return -EPERM;
  }

  r = fs_inode_read_dir(file->node->inode, buf, nbytes, &file->offset);

  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_fstat(struct File *file, struct stat *buf)
{
  int r;
  
  if (file->type != FD_INODE) 
    return -EBADF;

  fs_inode_lock(file->node->inode);
  r = fs_inode_stat(file->node->inode, buf);
  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_fchdir(struct File *file)
{
  int r;

  if (file->type != FD_INODE)
    return -EBADF;

  if ((r = fs_set_pwd(fs_path_duplicate(file->node))) != 0)
    fs_path_put(file->node);

  return r;
}

int
fs_chmod(const char *path, mode_t mode)
{
  struct PathNode *node;
  int r;

  if ((r = fs_lookup(path, 0, &node)) < 0)
    return r;

  fs_inode_lock(node->inode);
  r = fs_inode_chmod(node->inode, mode);
  fs_inode_unlock(node->inode);

  fs_path_put(node);

  return r;
}

int
fs_fchmod(struct File *file, mode_t mode)
{
  int r;

  if (file->type != FD_INODE) 
    return -EBADF;

  fs_inode_lock(file->node->inode);
  r = fs_inode_chmod(file->node->inode, mode);
  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_fchown(struct File *file, uid_t uid, gid_t gid)
{
  int r;

  if (file->type != FD_INODE) 
    return -EBADF;

  fs_inode_lock(file->node->inode);
  r = fs_inode_chown(file->node->inode, uid, gid);
  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_ioctl(struct File *file, int request, int arg)
{
  int r;
  
  if (file->type != FD_INODE) 
    return -EBADF;
  
  fs_inode_lock(file->node->inode);
  r = fs_inode_ioctl(file->node->inode, request, arg);
  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_select(struct File *file)
{
  int r;
  
  if (file->type != FD_INODE) 
    return -EBADF;
  
  fs_inode_lock(file->node->inode);
  r = fs_inode_select(file->node->inode);
  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_ftruncate(struct File *file, off_t length)
{
  int r;
  
  if (file->type != FD_INODE) 
    return -EBADF;

  fs_inode_lock(file->node->inode);
  r = fs_inode_truncate(file->node->inode, length);
  fs_inode_unlock(file->node->inode);

  return r;
}

int
fs_fsync(struct File *file)
{
  int r;
  
  if (file->type != FD_INODE) 
    return -EBADF;

  fs_inode_lock(file->node->inode);
  r = fs_inode_sync(file->node->inode);
  fs_inode_unlock(file->node->inode);

  return r;
}
