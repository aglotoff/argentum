#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kernel/fs/fs.h>
#include <kernel/fs/file.h>
#include <kernel/cprintf.h>

#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC)

int
fs_chmod(const char *path, mode_t mode)
{
  struct Inode *inode;
  int r;

  if ((r = fs_lookup_inode(path, 0, &inode)) < 0)
    return r;

  fs_inode_lock(inode);

  r = fs_inode_chmod_locked(inode, mode);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

ssize_t
fs_readlink(const char *path, char *buf, size_t bufsize)
{
  struct Inode *inode;
  int r;

  if ((r = fs_lookup_inode(path, 0, &inode)) < 0)
    return r;

  r = fs_inode_readlink(inode, buf, bufsize);

  fs_inode_put(inode);

  return r;
}

int
fs_access(const char *path, int amode)
{
  struct Inode *inode;
  int r;

  if ((r = fs_lookup_inode(path, 0, &inode)) < 0)
    return r;

  if (amode != F_OK)
    r = fs_inode_access(inode, amode);

  fs_inode_put(inode);

  return r;
}

int
fs_open(const char *path, int oflag, mode_t mode, struct File **file_store)
{
  struct File *file;
  struct PathNode *path_node;
  struct Inode *inode;
  int r;

  // TODO: ENFILE
  if ((r = file_alloc(&file)) != 0)
    return r;

  file->flags     = oflag & (STATUS_MASK | O_ACCMODE);
  file->type      = FD_INODE;
  file->node      = NULL;
  file->ref_count = 1;

  // TODO: the check and the file creation should be atomic
  if ((r = fs_lookup(path, 0, &path_node)) < 0)
    goto out1;

  if (path_node == NULL) {
    if (!(oflag & O_CREAT)) {
      r = -ENOENT;
      goto out1;
    }
    
    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);

    if ((r = fs_create(path, S_IFREG | mode, 0, &path_node)) < 0)
      goto out1;
    inode = fs_path_inode(path_node);
  } else {
    inode = fs_path_inode(path_node);
    fs_inode_lock(inode);

    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      r = -EEXIST;
      goto out2;
    }
  }

  if (S_ISDIR(inode->mode) && (oflag & O_WRONLY)) {
    r = -ENOTDIR;
    goto out2;
  }

  // TODO: O_NOCTTY
  // TODO: O_NONBLOCK

  // TODO: ENXIO
  // TODO: EROFS

  if ((oflag & O_WRONLY) && (oflag & O_TRUNC)) {
    if ((r = fs_inode_truncate_locked(inode, 0)) < 0)
      goto out2;
  }

  file->node = path_node;

  if (oflag & O_RDONLY) {
    // TODO: check group and other permissions
    if (!(inode->mode & S_IRUSR)) {
      r = -EPERM;
      goto out2;
    }
  }

  if (oflag & O_WRONLY) {
    // TODO: check group and other permissions
    if (!(inode->mode & S_IWUSR)) {
      r = -EPERM;
      goto out2;
    }
  }

  if (oflag & O_APPEND)
    file->offset = inode->size;

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  *file_store = file;

  return 0;

out2:
  fs_inode_unlock(inode);
  fs_inode_put(inode);
  fs_path_put(path_node);
out1:
  file_put(file);
  return r;
}

int
fs_close(struct File *file)
{
  if (file->type != FD_INODE)
    panic("not a file");

  // TODO: add a comment, when node can be NULL?
  if (file->node != NULL) 
    fs_path_put(file->node);

  return 0;
}

off_t
fs_seek(struct File *file, off_t offset, int whence)
{
  struct Inode *inode;
  off_t new_offset;

  if (file->type != FD_INODE)
    panic("not a file");

  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = file->offset + offset;
    break;
  case SEEK_END:
    inode = fs_path_inode(file->node);
    fs_inode_lock(inode);

    new_offset = inode->size + offset;

    fs_inode_unlock(inode);
    fs_inode_put(inode);

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
fs_read(struct File *file, uintptr_t va, size_t nbytes)
{
  struct Inode *inode;
  ssize_t r;

  if (file->type != FD_INODE)
    panic("not a file");

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_read_locked(inode, va, nbytes, &file->offset);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

ssize_t
fs_write(struct File *file, uintptr_t va, size_t nbytes)
{
  struct Inode *inode;
  ssize_t r;

  if (file->type != FD_INODE)
    panic("not a file");

  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  if (file->flags & O_APPEND)
    file->offset = inode->size;

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_write_locked(inode, va, nbytes, &file->offset);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

ssize_t
fs_getdents(struct File *file, uintptr_t va, size_t nbytes)
{
  struct Inode *inode;
  ssize_t r;

  if (file->type != FD_INODE)
    panic("not a file");

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_read_dir_locked(inode, va, nbytes, &file->offset);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_fstat(struct File *file, struct stat *buf)
{
  struct Inode *inode;
  int r;
  
  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_stat_locked(inode, buf);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_fchdir(struct File *file)
{
  int r;

  if (file->type != FD_INODE)
    panic("not a file");

  if ((r = fs_set_pwd(fs_path_duplicate(file->node))) != 0)
    fs_path_put(file->node);

  return r;
}

int
fs_fchmod(struct File *file, mode_t mode)
{
  struct Inode *inode;
  int r;

  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_chmod_locked(inode, mode);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_fchown(struct File *file, uid_t uid, gid_t gid)
{
  struct Inode *inode;
  int r;

  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_chown_locked(inode, uid, gid);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_ioctl(struct File *file, int request, int arg)
{
  struct Inode *inode;
  int r;
  
  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_ioctl_locked(inode, request, arg);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_select(struct File *file, struct timeval *timeout)
{
  struct Inode *inode;
  int r;
  
  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_select_locked(inode, timeout);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_ftruncate(struct File *file, off_t length)
{
  struct Inode *inode;
  int r;
  
  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_truncate_locked(inode, length);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_fsync(struct File *file)
{
  struct Inode *inode;
  int r;
  
  if (file->type != FD_INODE)
    panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_sync_locked(inode);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}
