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

#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC)

int
fs_chmod(const char *path, mode_t mode)
{
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve_inode(path, FS_LOOKUP_FOLLOW_LINKS, &inode)) < 0)
    return r;

  fs_inode_lock(inode);

  r = fs_inode_chmod_locked(inode, mode);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_access(const char *path, int amode)
{
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve_inode(path, FS_LOOKUP_FOLLOW_LINKS, &inode)) < 0)
    return r;

  if (amode != F_OK)
    r = fs_inode_access(inode, amode);

  fs_inode_put(inode);

  return r;
}

int
fs_utime(const char *path, struct utimbuf *times)
{
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve_inode(path, FS_LOOKUP_FOLLOW_LINKS, &inode)) < 0)
    return r;

  r = fs_inode_utime(inode, times);

  fs_inode_put(inode);

  return r;
}

//int eexists_cnt;

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

    inode = fs_inode_duplicate(fs_path_inode(path_node));
    fs_inode_lock(inode);
  } else {
    inode = fs_inode_duplicate(fs_path_inode(path_node));
    fs_inode_lock(inode);

    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      //k_warn("file already exists %s", path);
      //if (eexists_cnt > 4)
      //  k_panic("eeee");
      //eexists_cnt++;

      r = -EEXIST;
      goto out2;
    }

    if ((oflag & O_DIRECTORY) && !S_ISDIR(inode->mode)) {
      r = -ENOTDIR;
      goto out2;
    }
  }

  if (S_ISDIR(inode->mode) && (oflag & O_WRONLY)) {
    r = -ENOTDIR;
    goto out2;
  }

  // TODO: ENXIO
  // TODO: EROFS

  if ((oflag & O_WRONLY) && (oflag & O_TRUNC)) {
    if ((r = fs_inode_truncate_locked(inode, 0)) < 0)
      goto out2;
  }

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

  if ((r = fs_inode_open_locked(inode, oflag, mode)) < 0)
    goto out2;

  // REF(file->node)
  file->node  = fs_path_node_ref(path_node);
  file->inode = fs_inode_duplicate(inode);

  // UNREF(path_node)
  fs_path_node_unref(path_node);
  path_node = NULL;

  if (oflag & O_APPEND)
    file->offset = inode->size;

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  *file_store = file;

  return 0;

out2:
  fs_inode_unlock(inode);
  fs_inode_put(inode);

  // UNREF(path_node)
  fs_path_node_unref(path_node);
out1:
  file_put(file);
  return r;
}

int
fs_close(struct File *file)
{
  if (file->type != FD_INODE)
    k_panic("not a file");

  // TODO: add a comment, when node can be NULL?
  if (file->node != NULL) {
    // UNREF(file->node)
    fs_inode_put(file->inode);
    file->inode = NULL;

    fs_path_node_unref(file->node);
    file->node = NULL;
  }

  return 0;
}

off_t
fs_seek(struct File *file, off_t offset, int whence)
{
  struct Inode *inode;
  off_t new_offset;

  if (file->type != FD_INODE)
    k_panic("not a file");

  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = file->offset + offset;
    break;
  case SEEK_END:
    inode = fs_inode_duplicate(fs_path_inode(file->node));
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
    k_panic("not a file");

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  inode = fs_inode_duplicate(fs_path_inode(file->node));
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
    k_panic("not a file");

  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  inode = fs_inode_duplicate(fs_path_inode(file->node));

  r = fs_inode_write_locked(inode, va, nbytes, &file->offset, file->flags);

  fs_inode_put(inode);

  return r;
}

ssize_t
fs_getdents(struct File *file, uintptr_t va, size_t nbytes)
{
  struct Inode *inode;
  ssize_t r;

  if (file->type != FD_INODE)
    k_panic("not a file");

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  inode = fs_inode_duplicate(fs_path_inode(file->node));
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
    k_panic("not a file");

  inode = fs_inode_duplicate(fs_path_inode(file->node));
  fs_inode_lock(inode);

  r = fs_inode_stat_locked(inode, buf);

  // if (strcmp(file->node->name, "conftest.file") == 0 || strcmp(file->node->name, "configure") == 0)
  //   cprintf("[k] stat %s %lld %lld %lld\n", file->node->name, inode->atime, inode->ctime, inode->mtime);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_fchdir(struct File *file)
{
  struct PathNode *node;
  int r;

  if (file->type != FD_INODE)
    k_panic("not a file");

  // REF(node)
  node = fs_path_node_ref(file->node);

  r = fs_set_pwd(node);

  // UNREF(node)
  fs_path_node_unref(node);

  return r;
}

int
fs_fchmod(struct File *file, mode_t mode)
{
  struct Inode *inode;
  int r;

  if (file->type != FD_INODE)
    k_panic("not a file");

  inode = fs_inode_duplicate(fs_path_inode(file->node));
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
    k_panic("not a file");

  inode = fs_inode_duplicate(fs_path_inode(file->node));
  fs_inode_lock(inode);

  r = fs_inode_chown_locked(inode, uid, gid);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
fs_chown(const char *path, uid_t uid, gid_t gid)
{
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve_inode(path, FS_LOOKUP_FOLLOW_LINKS, &inode)) < 0)
    return r;

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
    k_panic("not a file");

  inode = fs_inode_duplicate(fs_path_inode(file->node));
  fs_inode_lock(inode);

  r = fs_inode_ioctl_locked(inode, request, arg);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

/*
 * ----- Pathname Operations -----
 */

ssize_t
fs_readlink(const char *path, uintptr_t va, size_t bufsize)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  fs_path_node_lock(node);
  r = fs_inode_readlink(fs_path_inode(node), va, bufsize);
  fs_path_node_unlock(node);

  return r;
}

/*
 * ----- File Operations ----- 
 */

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

int
fs_select(struct File *file, struct timeval *timeout)
{
  struct Inode *inode;
  int r;
  
  if (file->type != FD_INODE)
    k_panic("not a file");

  inode = fs_inode_duplicate(fs_path_inode(file->node));
  fs_inode_lock(inode);

  r = fs_inode_select_locked(inode, timeout);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}
