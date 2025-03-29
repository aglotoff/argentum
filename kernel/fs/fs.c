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
fs_utime(const char *path, struct utimbuf *times)
{
  struct Inode *inode;
  int r;

  if ((r = fs_lookup_inode(path, 0, &inode)) < 0)
    return r;

  r = fs_inode_utime(inode, times);

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
    fs_inode_lock(inode);
  } else {
    inode = fs_path_inode(path_node);
    fs_inode_lock(inode);

    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
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

  file->node = path_node;

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
    k_panic("not a file");

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
    k_panic("not a file");

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
    k_panic("not a file");

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
    k_panic("not a file");

  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  if (file->flags & O_APPEND)
    file->offset = inode->size;

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
    k_panic("not a file");

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
    k_panic("not a file");

  inode = fs_path_inode(file->node);
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
  int r;

  if (file->type != FD_INODE)
    k_panic("not a file");

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
    k_panic("not a file");

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
    k_panic("not a file");

  inode = fs_path_inode(file->node);
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

  if ((r = fs_lookup_inode(path, 0, &inode)) < 0)
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
    k_panic("not a file");

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
    k_panic("not a file");

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
    k_panic("not a file");

  inode = fs_path_inode(file->node);
  fs_inode_lock(inode);

  r = fs_inode_sync_locked(inode);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

void
fs_service_task(void *arg)
{
  struct FS *fs = (struct FS *) arg;
  struct FSMessage *msg;

  while (k_mailbox_receive(&fs->mbox, (void *) &msg) >= 0) {
    k_assert(msg != NULL);

    switch (msg->type) {
    case FS_MSG_INODE_READ:
      msg->u.inode_read.r = fs->ops->inode_read(msg->sender, msg->u.inode_read.inode);
      break;
    case FS_MSG_INODE_WRITE:
      msg->u.inode_write.r = fs->ops->inode_write(msg->sender, msg->u.inode_write.inode);
      break;
    case FS_MSG_INODE_DELETE:
      fs->ops->inode_delete(msg->sender, msg->u.inode_delete.inode);
      break;
    case FS_MSG_TRUNC:
      fs->ops->trunc(msg->sender, msg->u.trunc.inode, msg->u.trunc.length);
      break;
    case FS_MSG_LOOKUP:
      msg->u.lookup.r = fs->ops->lookup(msg->sender, msg->u.lookup.dir, msg->u.lookup.name);
      break;
    case FS_MSG_READ:
      msg->u.read.r = fs->ops->read(msg->sender,
                                    msg->u.read.inode,
                                    msg->u.read.va, msg->u.read.nbyte,
                                    msg->u.read.off);
      break;
    case FS_MSG_WRITE:
      msg->u.write.r = fs->ops->write(msg->sender,
                                      msg->u.write.inode,
                                      msg->u.write.va,
                                      msg->u.write.nbyte,
                                      msg->u.write.off);
      break;
    case FS_MSG_READDIR:
      msg->u.readdir.r = fs->ops->readdir(msg->sender,
                                          msg->u.readdir.inode,
                                          msg->u.readdir.buf,
                                          msg->u.readdir.func,
                                          msg->u.readdir.off);
      break;
    case FS_MSG_MKDIR:
      msg->u.mkdir.r = fs->ops->mkdir(msg->sender, msg->u.mkdir.dir,
                                      msg->u.mkdir.name,
                                      msg->u.mkdir.mode,
                                      msg->u.mkdir.istore);
      break;
    case FS_MSG_CREATE:
      msg->u.create.r = fs->ops->create(msg->sender, msg->u.create.dir,
                                        msg->u.create.name,
                                        msg->u.create.mode,
                                        msg->u.create.istore);
      break;
    case FS_MSG_MKNOD:
      msg->u.mknod.r = fs->ops->mknod(msg->sender,
                                      msg->u.mknod.dir,
                                      msg->u.mknod.name,
                                      msg->u.mknod.mode,
                                      msg->u.mknod.dev,
                                      msg->u.mknod.istore);
      break;
    case FS_MSG_LINK:
      msg->u.link.r = fs->ops->link(msg->sender,
                                    msg->u.link.dir,
                                    msg->u.link.name,
                                    msg->u.link.inode);
      break;
    case FS_MSG_UNLINK:
      msg->u.unlink.r = fs->ops->unlink(msg->sender,
                                        msg->u.unlink.dir,
                                        msg->u.unlink.inode,
                                        msg->u.unlink.name);
      break;    
    case FS_MSG_RMDIR:
      msg->u.rmdir.r = fs->ops->rmdir(msg->sender,
                                      msg->u.rmdir.dir,
                                      msg->u.rmdir.inode,
                                      msg->u.rmdir.name);
      break;
    default:
      k_panic("bad msg type %d\n", msg->type);
      break;
    }

    k_semaphore_put(&msg->sem);
  }

  k_panic("error");
}

struct FS *
fs_create_service(char *name, dev_t dev, void *extra, struct FSOps *ops)
{
  struct FS *fs;
  int i;

  if ((fs = (struct FS *) k_malloc(sizeof(struct FS))) == NULL)
    k_panic("cannot allocate FS");
  
  fs->name  = name;
  fs->dev   = dev;
  fs->extra = extra;
  fs->ops   = ops;

  k_mailbox_create(&fs->mbox, sizeof(void *), fs->mbox_buf, sizeof fs->mbox_buf);

  for (i = 0; i < FS_MBOX_CAPACITY; i++) {
    struct Page *kstack;

    if ((kstack = page_alloc_one(0, 0)) == NULL)
      k_panic("out of memory");

    kstack->ref_count++;

    k_task_create(&fs->tasks[i], NULL, fs_service_task, fs, page2kva(kstack), PAGE_SIZE, 0);
    k_task_resume(&fs->tasks[i]);
  }

  return fs;
}

int
fs_send_recv(struct FS *fs, struct FSMessage *msg)
{
  struct FSMessage *msg_ptr = msg;
  int r;

  k_semaphore_create(&msg->sem, 0);
  msg->sender = thread_current();

  if (k_mailbox_send(&fs->mbox, &msg_ptr) < 0)
    k_panic("fail");

  if ((r = k_semaphore_get(&msg->sem, K_SLEEP_UNINTERUPTIBLE)) < 0)
    k_panic("fail %d", r);

  return 0;
}
