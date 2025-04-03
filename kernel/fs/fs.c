#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kernel/fs/fs.h>
#include <kernel/fs/file.h>
#include <kernel/console.h>
#include <kernel/time.h>
#include <kernel/page.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/dev.h>

#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC)

int
fs_open(const char *path, int oflag, mode_t mode, struct File **file_store)
{
  struct File *file;
  struct PathNode *path_node;
  struct Inode *inode;
  int r, flags;

  // TODO: O_NONBLOCK

  //if (oflag & O_SEARCH) k_panic("O_SEARCH %s", path);
  //if (oflag & O_EXEC) k_panic("O_EXEC %s", path);
  //if (oflag & O_NOFOLLOW) k_panic("O_NOFOLLOW %s", path);
  if (oflag & O_SYNC) k_panic("O_SYNC %s", path);
  if (oflag & O_DIRECT) k_panic("O_DIRECT %s", path);

  // TODO: ENFILE
  if ((r = file_alloc(&file)) != 0)
    return r;

  file->flags     = oflag & (STATUS_MASK | O_ACCMODE);
  file->type      = FD_INODE;
  file->node      = NULL;
  file->inode     = NULL;
  file->rdev      = -1;
  file->ref_count = 1;

  flags = FS_LOOKUP_FOLLOW_LINKS;
  if ((oflag & O_EXCL) && (oflag & O_CREAT))
    flags &= ~FS_LOOKUP_FOLLOW_LINKS;
  if (oflag & O_NOFOLLOW)
    flags &= ~FS_LOOKUP_FOLLOW_LINKS;

  // TODO: the check and the file creation should be atomic
  // REF(path_node)
  if ((r = fs_path_resolve(path, flags, &path_node)) < 0)
    goto out1;

  if (path_node == NULL) {
    if (!(oflag & O_CREAT)) {
      r = -ENOENT;
      goto out1;
    }
    
    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);

    // REF(path_node)
    if ((r = fs_create(path, S_IFREG | mode, 0, &path_node)) < 0)
      goto out1;
  } else {
    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      r = -EEXIST;
      goto out2;
    }
  }

  inode = fs_path_inode(path_node);

  if ((r = fs_inode_open(inode, oflag, &file->rdev)) < 0)
    goto out2;

  if (file->rdev >= 0) {
    struct CharDev *d = dev_lookup_char(file->rdev);

    if (d == NULL) {
      r = -ENODEV;
      goto out2;
    }

    if ((r = d->open(thread_current(), file->rdev, oflag, mode)) < 0)
      goto out2;
  }

  // REF(file->node)
  file->node  = fs_path_node_ref(path_node);
  file->inode = fs_inode_duplicate(inode);

  // UNREF(path_node)
  fs_path_node_unref(path_node);
  path_node = NULL;

  if (oflag & O_APPEND)
    file->offset = inode->size;

  *file_store = file;

  return 0;
out2:
  // UNREF(path_node)
  fs_path_node_unref(path_node);
out1:
  file_put(file);
  return r;
}

/*
 * ----- Pathname Operations -----
 */

int
fs_access(const char *path, int amode)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  if (amode != F_OK)
    r = fs_inode_access(fs_path_inode(node), amode);

  fs_path_node_unref(node);

  return r;
}
 
int
fs_chdir(const char *path)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  r = fs_path_set_cwd(node);

  fs_path_node_unref(node);

  return r;
}

int
fs_chmod(const char *path, mode_t mode)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  r = fs_inode_chmod(fs_path_inode(node), mode);

  fs_path_node_unref(node);

  return r;
}

int
fs_chown(const char *path, uid_t uid, gid_t gid)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  r = fs_inode_chown(fs_path_inode(node), uid, gid);

  fs_path_node_unref(node);

  return r;
}

ssize_t
fs_readlink(const char *path, uintptr_t va, size_t bufsize)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  r = fs_inode_readlink(fs_path_inode(node), va, bufsize);

  fs_path_node_unref(node);

  return r;
}

int
fs_utime(const char *path, struct utimbuf *times)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  r = fs_inode_utime(fs_path_inode(node), times);

  fs_path_node_unref(node);

  return r;
}

/*
 * ----- File Operations ----- 
 */

int
fs_close(struct File *file)
{
  k_assert(file->type == FD_INODE);

  // TODO: add a comment, when node can be NULL?
  if (file->node != NULL) {
    fs_inode_put(file->inode);
    file->inode = NULL;

    fs_path_node_unref(file->node);
    file->node = NULL;
  }

  return 0;
}
 
int
fs_fchdir(struct File *file)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  return fs_path_set_cwd(file->node);
}

int
fs_fchmod(struct File *file, mode_t mode)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  return fs_inode_chmod(file->inode, mode);
}

int
fs_fchown(struct File *file, uid_t uid, gid_t gid)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  return fs_inode_chown(file->inode, uid, gid);
}

int
fs_fstat(struct File *file, struct stat *buf)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  return fs_inode_stat(file->inode, buf);
}

int
fs_fsync(struct File *file)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  return fs_inode_sync(file->inode);
}

int
fs_ftruncate(struct File *file, off_t length)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  return fs_inode_truncate(file->inode, length);
}

ssize_t
fs_getdents(struct File *file, uintptr_t va, size_t nbytes)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  return fs_inode_read_dir(file->inode, va, nbytes, &file->offset);
}

int
fs_ioctl(struct File *file, int request, int arg)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if (file->rdev >= 0) {
    struct CharDev *d = dev_lookup_char(file->rdev);

    if (d == NULL)
      return -ENODEV;

    return d->ioctl(thread_current(), file->rdev, request, arg);
  }

  return fs_inode_ioctl(file->inode, request, arg);
}

ssize_t
fs_read(struct File *file, uintptr_t va, size_t nbytes)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  // Read from the corresponding device
  if (file->rdev >= 0) {
    struct CharDev *d = dev_lookup_char(file->rdev);

    if (d == NULL)
      return -ENODEV;

    return d->read(thread_current(), file->rdev, va, nbytes);
  }

  return fs_inode_read(file->inode, va, nbytes, &file->offset);
}

off_t
fs_seek(struct File *file, off_t offset, int whence)
{
  off_t new_offset;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = file->offset + offset;
    break;
  case SEEK_END:
    new_offset = fs_inode_seek(file->inode, offset);
    break;
  default:
    return -EINVAL;
  }

  if (new_offset < 0)
    return -EOVERFLOW;

  file->offset = new_offset;

  return new_offset;
}

int
fs_select(struct File *file, struct timeval *timeout)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if (file->rdev >= 0) {
    struct CharDev *d = dev_lookup_char(file->rdev);

    if (d == NULL)
      return -ENODEV;

    return d->select(thread_current(), file->rdev, timeout);
  }

  return fs_inode_select(file->inode, timeout);
}

ssize_t
fs_write(struct File *file, uintptr_t va, size_t nbytes)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  // Write to the corresponding device
  if (file->rdev >= 0) {
    struct CharDev *d = dev_lookup_char(file->rdev);

    if (d == NULL)
      return -ENODEV;

    return d->write(thread_current(), file->rdev, va, nbytes);
  }

  return fs_inode_write(file->inode, va, nbytes, &file->offset, file->flags);
}
