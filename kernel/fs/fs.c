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

/*
 * ----- Pathname Operations -----
 */

int
fs_access(const char *path, int amode)
{
  struct FSMessage msg;
  struct PathNode *node;
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  inode = fs_path_inode(node);

  msg.type = FS_MSG_ACCESS;
  msg.u.access.inode = inode;
  msg.u.access.amode = amode;

  fs_send_recv(inode->fs, &msg);

  fs_path_node_unref(node);

  return msg.u.access.r;
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
  struct FSMessage msg;
  struct PathNode *node;
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  inode = fs_path_inode(node);

  msg.type = FS_MSG_CHMOD;
  msg.u.chmod.inode = inode;
  msg.u.chmod.mode  = mode;

  fs_send_recv(inode->fs, &msg);

  fs_path_node_unref(node);

  return msg.u.chmod.r;
}

int
fs_chown(const char *path, uid_t uid, gid_t gid)
{
  struct FSMessage msg;
  struct PathNode *node;
  struct Inode *inode;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  inode = fs_path_inode(node);

  msg.type = FS_MSG_CHOWN;
  msg.u.chown.inode = inode;
  msg.u.chown.uid   = uid;
  msg.u.chown.gid   = gid;

  fs_send_recv(inode->fs, &msg);

  fs_path_node_unref(node);

  return msg.u.chown.r;
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
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  msg.type = FS_MSG_FCHMOD;
  msg.u.fchmod.file = file;
  msg.u.fchmod.mode = mode;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.fchmod.r;
}

int
fs_fchown(struct File *file, uid_t uid, gid_t gid)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  msg.type = FS_MSG_FCHOWN;
  msg.u.fchown.file = file;
  msg.u.fchown.uid  = uid;
  msg.u.fchown.gid  = gid;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.fchown.r;
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
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  msg.type = FS_MSG_TRUNC;
  msg.u.trunc.file   = file;
  msg.u.trunc.length = length;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.trunc.r;
}

ssize_t
fs_getdents(struct File *file, uintptr_t va, size_t nbyte)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  msg.type = FS_MSG_READDIR;
  msg.u.readdir.file = file;
  msg.u.readdir.va    = va;
  msg.u.readdir.nbyte = nbyte;
  
  fs_send_recv(file->inode->fs, &msg);

  return msg.u.readdir.r;
}

int
fs_ioctl(struct File *file, int request, int arg)
{
  struct FSMessage msg;
  
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if (file->rdev >= 0)
    return dev_ioctl(thread_current(), file->rdev, request, arg);

  msg.type = FS_MSG_IOCTL;
  msg.u.ioctl.file    = file;
  msg.u.ioctl.request = request;
  msg.u.ioctl.arg     = arg;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.ioctl.r;
}

#define OPEN_TIME_FLAGS (O_CREAT | O_EXCL | O_DIRECTORY | O_NOFOLLOW | O_NOCTTY | O_TRUNC)

int
fs_open(const char *path, int oflag, mode_t mode, struct File **file_store)
{
  struct FSMessage msg;
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

  file->flags     = oflag & ~OPEN_TIME_FLAGS;
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

  msg.type = FS_MSG_OPEN;
  msg.u.open.file  = file;
  msg.u.open.inode = inode;
  msg.u.open.oflag = oflag;

  fs_send_recv(inode->fs, &msg);

  if ((r = msg.u.open.r) < 0)
    goto out2;

  if (file->rdev >= 0) {
    if ((r = dev_open(thread_current(), file->rdev, oflag, mode)) < 0)
      goto out2;
  }

  // REF(file->node)
  file->node  = fs_path_node_ref(path_node);

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

ssize_t
fs_read(struct File *file, uintptr_t va, size_t nbytes)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if (file->rdev >= 0)
    return dev_read(thread_current(), file->rdev, va, nbytes);

  msg.type = FS_MSG_READ;
  msg.u.read.file  = file;
  msg.u.read.va    = va;
  msg.u.read.nbyte = nbytes;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.read.r;
}

off_t
fs_seek(struct File *file, off_t offset, int whence)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  msg.type = FS_MSG_SEEK;
  msg.u.seek.file   = file;
  msg.u.seek.offset = offset;
  msg.u.seek.whence = whence;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.seek.r;
}

int
fs_select(struct File *file, struct timeval *timeout)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if (file->rdev >= 0)
    return dev_select(thread_current(), file->rdev, timeout);

  msg.type = FS_MSG_SELECT;
  msg.u.select.file    = file;
  msg.u.select.timeout = timeout;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.select.r;
}

ssize_t
fs_write(struct File *file, uintptr_t va, size_t nbytes)
{
  struct FSMessage msg;
  
  k_assert(file->ref_count > 0);
  k_assert(file->type == FD_INODE);
  k_assert(file->inode != NULL);

  if (file->rdev >= 0)
    return dev_write(thread_current(), file->rdev, va, nbytes);

  msg.type = FS_MSG_WRITE;
  msg.u.write.file  = file;
  msg.u.write.va    = va;
  msg.u.write.nbyte = nbytes;

  fs_send_recv(file->inode->fs, &msg);

  return msg.u.write.r;
}
