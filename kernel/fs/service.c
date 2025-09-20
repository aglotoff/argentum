#include <dirent.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/ipc/channel.h>
#include <kernel/page.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/time.h>
#include <kernel/vmspace.h>
#include <kernel/dev.h>
#include <kernel/hash.h>

static struct IpcRequest *
ipc_request_create(struct IpcMessage *msg)
{
  struct IpcRequest *req;

  if ((req = k_malloc(sizeof *req)) == NULL)
    return NULL;

  k_semaphore_create(&req->sem, 0);
  k_spinlock_init(&req->lock, "req");
  
  req->sender = NULL;
  req->channel = NULL;
  req->msg = msg;
  req->ref_count = 1;

  return req;
}

static void
ipc_request_destroy(struct IpcRequest *req)
{
  int count;

  k_assert(req->ref_count > 0);

  k_spinlock_acquire(&req->lock);

  req->channel = NULL;
  req->msg = NULL;
  req->sender = NULL;

  count = --req->ref_count;

  k_spinlock_release(&req->lock);

  if (count == 0) {
    k_free(req);
  }
}

static void
ipc_request_dup(struct IpcRequest *req)
{
  k_spinlock_acquire(&req->lock);
  req->ref_count++;
  k_spinlock_release(&req->lock);
}

static void
ipc_request_reply(struct IpcRequest *req)
{
  k_semaphore_put(&req->sem);
  ipc_request_destroy(req);
}

// File data

#define NBUCKET   256

static struct {
  struct KListLink table[NBUCKET];
  struct KSpinLock lock;
} file_hash;

static struct File *
get_channel_file(struct Channel *channel)
{
  struct KListLink *l;

  k_spinlock_acquire(&file_hash.lock);

  HASH_FOREACH_ENTRY(file_hash.table, l, (uintptr_t) channel) {
    struct File *file;
    
    file = KLIST_CONTAINER(l, struct File, hash_link);
    if (file->channel == channel) {
      k_spinlock_release(&file_hash.lock);
      return file;
    }
  }

  k_spinlock_release(&file_hash.lock);
  return NULL;
}

static void
set_channel_file(struct File *file,
                 struct Channel *channel)
{
  file->channel = channel;
  k_list_null(&file->hash_link);

  k_spinlock_acquire(&file_hash.lock);
  HASH_PUT(file_hash.table, &file->hash_link, (uintptr_t) channel);
  k_spinlock_release(&file_hash.lock);
}


static int
fs_filldir(void *buf, ino_t ino, const char *name, size_t name_len)
{
  struct dirent *dp = (struct dirent *) buf;

  dp->d_reclen  = name_len + offsetof(struct dirent, d_name) + 1;
  dp->d_ino     = ino;
  memmove(&dp->d_name[0], name, name_len);
  dp->d_name[name_len] = '\0';

  return dp->d_reclen;
}

/*
 * ----- Inode Operations -----
 */

void
do_access(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;

  mode_t amode = msg->u.access.amode;
  ino_t ino = msg->u.access.ino;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  msg->r = 0;

  if (msg->u.access.amode == F_OK) {
    fs_inode_put(inode);
    ipc_request_reply(req);
    return;
  }

  fs_inode_lock(inode);

  if ((amode & R_OK) && !fs_inode_permission(sender, inode, FS_PERM_READ, 1))
    msg->r = -EPERM;
  if ((amode & W_OK) && !fs_inode_permission(sender, inode, FS_PERM_WRITE, 1))
    msg->r = -EPERM;
  if ((amode & X_OK) && !fs_inode_permission(sender, inode, FS_PERM_EXEC, 1))
    msg->r = -EPERM;

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

void
do_chdir(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t ino = msg->u.chdir.ino;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);

  if (!S_ISDIR(inode->mode)) {
    msg->r = -ENOTDIR;
  } else if (!fs_inode_permission(sender, inode, FS_PERM_EXEC, 0)) {
    msg->r = -EPERM;
  } else {
    msg->r = 0;
  }

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

#define CHMOD_MASK  (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)

int
do_inode_chmod(struct FS *, struct Thread *sender,
               struct Inode *inode, mode_t mode)
{
  uid_t euid = sender ? sender->process->euid : 0;

  fs_inode_lock(inode);

  if ((euid != 0) && (inode->uid != euid)) {
    fs_inode_unlock(inode);
    return -EPERM;
  }

  // TODO: additional permission checks

  inode->mode = (inode->mode & ~CHMOD_MASK) | (mode & CHMOD_MASK);
  inode->ctime = time_get_seconds();
  inode->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(inode);

  return 0;
}

void
do_chmod(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  ino_t ino = msg->u.chmod.ino;
  mode_t mode = msg->u.chmod.mode;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  msg->r = do_inode_chmod(fs, req->sender, inode, mode);

  fs_inode_put(inode);

  ipc_request_reply(req);
}

static int
do_chown_locked(struct FS *, struct Thread *sender,
                struct Inode *inode,
                uid_t uid,
                gid_t gid)
{
  uid_t euid = sender ? sender->process->euid : 0;
  uid_t egid = sender ? sender->process->egid : 0;

  if ((uid != (uid_t) -1) &&
      (euid != 0) &&
      (euid != inode->uid))
    return -EPERM;

  if ((gid != (gid_t) -1) &&
      (euid != 0) &&
      (euid != inode->uid) &&
      ((uid != (uid_t) -1) || (egid != inode->gid)))
    return -EPERM;

  if (uid != (uid_t) -1)
    inode->uid = uid;

  if (gid != (gid_t) -1)
    inode->gid = gid;

  return 0;
}

int
do_inode_chown(struct FS *fs, struct Thread *sender,
               struct Inode *inode,
               uid_t uid, gid_t gid)
{
  int r;

  fs_inode_lock(inode);
  r = do_chown_locked(fs, sender, inode, uid, gid);
  fs_inode_unlock(inode);

  return r;
}

void
do_chown(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  ino_t ino = msg->u.chown.ino;
  uid_t uid = msg->u.chown.uid;
  gid_t gid = msg->u.chown.gid;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  msg->r = do_inode_chown(fs, req->sender, inode, uid, gid);

  fs_inode_put(inode);

  ipc_request_reply(req);
}

static int
do_create_locked(struct FS *fs, struct Thread *sender,
                 struct Inode *dir,
                 char *name,
                 mode_t mode,
                 dev_t dev,
                 struct Inode **istore)
{
  struct Inode *inode;

  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  inode = fs->ops->lookup(sender, dir, name);

  if (inode != NULL) {
    fs_inode_put(inode);
    return -EEXIST;
  }

  switch (mode & S_IFMT) {
    case S_IFDIR:
      return fs->ops->mkdir(sender, dir, name, mode, istore);
    case S_IFREG:
      return fs->ops->create(sender, dir, name, mode, istore);
    default:
      return fs->ops->mknod(sender, dir, name, mode, dev, istore);
  }
}

void
do_create(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  ino_t dir_ino = msg->u.create.dir_ino;
  char *name = msg->u.create.name;
  mode_t mode = msg->u.create.mode;
  dev_t dev  = msg->u.create.dev;
  ino_t *istore  = msg->u.create.istore;

  // TODO: verify existence
  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = NULL;

  fs_inode_lock(dir);
  msg->r = do_create_locked(fs, req->sender, dir, name, mode, dev, &inode);
  fs_inode_unlock(dir);

  if (inode != NULL) {
    if (istore)
      *istore = inode->ino;
    fs_inode_put(inode);
  }

  fs_inode_put(dir);

  ipc_request_reply(req);
}

static int
do_link_locked(struct FS *fs, struct Thread *sender,
               struct Inode *dir,
               char *name,
               struct Inode *inode)
{
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;

  if (inode->nlink >= LINK_MAX)
    return -EMLINK;

  if (dir->dev != inode->dev)
    return -EXDEV;

  return fs->ops->link(sender, dir, name, inode);
}

void
do_link(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  ino_t dir_ino = msg->u.link.dir_ino;
  char *name = msg->u.link.name;
  ino_t ino = msg->u.link.ino;

  // TODO: verify existence
  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);
  msg->r = do_link_locked(fs, req->sender, dir, name, inode);
  fs_inode_unlock_two(dir, inode);

  fs_inode_put(dir);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

void
do_lookup(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;

  ino_t dir_ino = msg->u.lookup.dir_ino;
  const char *name = msg->u.lookup.name;
  int flags = msg->u.lookup.flags;
  ino_t *istore = msg->u.lookup.istore;

  struct Inode *inode, *dir;

  dir = fs->ops->inode_get(fs, dir_ino);

  fs_inode_lock(dir);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock(dir);
    fs_inode_put(dir);
    msg->r = -ENOTDIR;
    ipc_request_reply(req);
    return;
  }

  if (!fs_inode_permission(sender, dir, FS_PERM_READ, flags & FS_LOOKUP_REAL)) {
    fs_inode_unlock(dir);
    fs_inode_put(dir);
    msg->r = -EPERM;
    ipc_request_reply(req);
    return;
  }
    
  inode = fs->ops->lookup(sender, dir, name);

  if (istore != NULL)
    *istore = inode ? inode->ino : 0;
  if (inode != NULL)
    fs_inode_put(inode);

  fs_inode_unlock(dir);
  fs_inode_put(dir);

  msg->r = 0;

  ipc_request_reply(req);
}

void
do_readlink(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t ino = msg->u.readlink.ino;
  uintptr_t va = msg->u.readlink.va;
  size_t nbyte = msg->u.readlink.nbyte;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);

  if (!fs_inode_permission(sender, inode, FS_PERM_READ, 0)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    msg->r = -EPERM;
    ipc_request_reply(req);
    return;
  }
    
  if (!S_ISLNK(inode->mode)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    msg->r = -EINVAL;
    ipc_request_reply(req);
    return;
  }

  msg->r = fs->ops->readlink(sender, inode, va, nbyte);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

void
do_rmdir(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t dir_ino = msg->u.rmdir.dir_ino;
  ino_t ino = msg->u.rmdir.ino;
  const char *name = msg->u.rmdir.name;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    msg->r = -ENOTDIR;
    ipc_request_reply(req);
    return;
  }

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    msg->r = -EPERM;
    ipc_request_reply(req);
    return;
  }

  // TODO: Allow links to directories?
  if (!S_ISDIR(inode->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    msg->r = -EPERM;
    ipc_request_reply(req);
    return;
  }

  msg->r = fs->ops->rmdir(sender, dir, inode, name);

  fs_inode_unlock_two(dir, inode);
  fs_inode_put(dir);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

int
do_inode_stat(struct FS *, struct Thread *,
              struct Inode *inode,
              struct stat *buf)
{
  fs_inode_lock(inode);
  
  buf->st_dev          = inode->dev;
  buf->st_ino          = inode->ino;
  buf->st_mode         = inode->mode;
  buf->st_nlink        = inode->nlink;
  buf->st_uid          = inode->uid;
  buf->st_gid          = inode->gid;
  buf->st_rdev         = inode->rdev;
  buf->st_size         = inode->size;
  buf->st_atim.tv_sec  = inode->atime;
  buf->st_atim.tv_nsec = 0;
  buf->st_mtim.tv_sec  = inode->mtime;
  buf->st_mtim.tv_nsec = 0;
  buf->st_ctim.tv_sec  = inode->ctime;
  buf->st_ctim.tv_nsec = 0;
  buf->st_blocks       = inode->size / S_BLKSIZE;
  buf->st_blksize      = S_BLKSIZE;

  fs_inode_unlock(inode);

  return 0;
}

void
do_stat(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t ino = msg->u.stat.ino;
  struct stat *buf = msg->u.stat.buf;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  msg->r = do_inode_stat(fs, sender, inode, buf);
  
  fs_inode_put(inode);

  ipc_request_reply(req);
}

static int
do_symlink_locked(struct FS *fs, struct Thread *sender,
                  struct Inode *dir,
                  char *name,
                  mode_t mode,
                  const char *path,
                  struct Inode **istore)
{
  struct Inode *inode;

  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  inode = fs->ops->lookup(sender, dir, name);

  if (inode != NULL) {
    fs_inode_put(inode);
    return -EEXIST;
  }

  return fs->ops->symlink(sender, dir, name, S_IFLNK | mode, path, istore);
}

void
do_symlink(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t dir_ino = msg->u.symlink.dir_ino;
  char *name = msg->u.symlink.name;
  mode_t mode = msg->u.symlink.mode;
  const char *link_path = msg->u.symlink.path;
  ino_t *istore = msg->u.symlink.istore;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = NULL;

  fs_inode_lock(dir);
  msg->r = do_symlink_locked(fs, sender, dir, name, mode, link_path, &inode);
  fs_inode_unlock(dir);

  if (inode != NULL) {
    if (istore)
      *istore = inode->ino;
    fs_inode_put(inode);
  }

  fs_inode_put(dir);

  ipc_request_reply(req);
}

void
do_unlink(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t dir_ino = msg->u.unlink.dir_ino;
  ino_t ino = msg->u.unlink.ino;
  const char *name = msg->u.unlink.name;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    msg->r = -ENOTDIR;
    ipc_request_reply(req);
    return;
  }

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    msg->r = -EPERM;
    ipc_request_reply(req);
    return;
  }

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    msg->r = -EPERM;
    ipc_request_reply(req);
    return;
  }

  msg->r = fs->ops->unlink(sender, dir, inode, name);

  fs_inode_unlock_two(dir, inode);
  fs_inode_put(dir);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

void
do_utime(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  ino_t ino = msg->u.utime.ino;
  struct utimbuf *times = msg->u.utime.times;

  struct Inode *inode = fs->ops->inode_get(fs, ino);
  uid_t euid = sender ? sender->process->euid : 0;

  fs_inode_lock(inode);

  if (times == NULL) {
    if ((euid != 0) &&
        (euid != inode->uid) &&
        !fs_inode_permission(sender, inode, FS_PERM_WRITE, 1)) {
      msg->r = -EPERM;
      goto out;
    }

    inode->atime = time_get_seconds();
    inode->mtime = time_get_seconds();

    inode->flags |= FS_INODE_DIRTY;
    msg->r = 0;
  } else {
    if ((euid != 0) ||
       ((euid != inode->uid) ||
         !fs_inode_permission(sender, inode, FS_PERM_WRITE, 1))) {
      msg->r = -EPERM;
      goto out;
    }

    inode->atime = times->actime;
    inode->mtime = times->modtime;

    inode->flags |= FS_INODE_DIRTY;
    msg->r = 0;
  }

out:
  fs_inode_unlock(inode);
  fs_inode_put(inode);

  ipc_request_reply(req);
}

/*
 * ----- File Operations -----
 */

void
do_close(struct FS *, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct File *file = get_channel_file(req->channel);

  if (file != NULL) {
    k_assert(file->inode != NULL);

    fs_inode_put(file->inode);

    k_spinlock_acquire(&file_hash.lock);
    k_list_remove(&file->hash_link);
    k_spinlock_release(&file_hash.lock);

    k_free(file);
  }

  msg->r = 0;

  ipc_request_reply(req);
}

void
do_fchmod(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  mode_t mode = msg->u.fchmod.mode;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    msg->r = do_inode_chmod(fs, sender, file->inode, mode);
  }

  ipc_request_reply(req);
}

void
do_fchown(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  uid_t uid = msg->u.fchown.uid;
  gid_t gid = msg->u.fchown.gid;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    msg->r = do_inode_chown(fs, sender, file->inode, uid, gid);
  }

  ipc_request_reply(req);
}

void
do_fstat(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  struct stat *buf = msg->u.fstat.buf;
  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    msg->r = do_inode_stat(fs, sender, file->inode, buf);
  }

  ipc_request_reply(req);
}

void
do_fsync(struct FS *, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    // FIXME: fsync
    fs_inode_unlock(file->inode);
    msg->r = 0;
  }

  ipc_request_reply(req);
}

void
do_ioctl(struct FS *, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);
  int request = msg->u.ioctl.request;
  int arg = msg->u.ioctl.arg;

  if (file->rdev >= 0) {
    msg->r = dev_ioctl(sender, file->rdev, request, arg);
  } else if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    // FIXME: ioctl
    fs_inode_unlock(file->inode);
    msg->r = -ENOTTY;
  }

  ipc_request_reply(req);
}

static int
do_open_locked(struct FS *fs, struct Thread *sender,
               struct Channel *channel, 
               struct Inode *inode,
               int oflag)
{
  if ((oflag & O_DIRECTORY) && !S_ISDIR(inode->mode))
    return -ENOTDIR;

  if (S_ISDIR(inode->mode) && (oflag & O_WRONLY))
    return -ENOTDIR;

  if (oflag & O_RDONLY) {
    // TODO: check group and other permissions
    if (!(inode->mode & S_IRUSR))
      return -EPERM;
  }

  if (oflag & O_WRONLY) {
    // TODO: check group and other permissions
    if (!(inode->mode & S_IWUSR))
      return -EPERM;
  }

  if ((oflag & O_WRONLY) && (oflag & O_TRUNC)) {
    fs->ops->trunc(sender, inode, 0);

    inode->size = 0;
    inode->ctime = inode->mtime = time_get_seconds();
    inode->flags |= FS_INODE_DIRTY;
  }

  struct File *file = (struct File *) k_malloc(sizeof(struct File));
  k_assert(file != NULL);

  set_channel_file(file, channel);

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode))
    file->rdev = inode->rdev;
  else
    file->rdev = -1;

  if (oflag & O_APPEND)
    file->offset = inode->size;
  else
    file->offset = 0;

  file->inode = fs_inode_duplicate(inode);

  return 0;
}

void
do_open(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  struct Channel *channel = req->channel;
  ino_t ino = msg->u.open.ino;
  int oflag = msg->u.open.oflag;
  mode_t mode = msg->u.open.mode;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);
  msg->r = do_open_locked(fs, sender, channel, inode, oflag);
  fs_inode_unlock(inode);

  fs_inode_put(inode);

  if (msg->r == 0) {
    struct File *file = get_channel_file(req->channel);
    k_assert(file != NULL);

    if (file->rdev >= 0) {
      msg->r = dev_open(sender, file->rdev, oflag, mode);
    }
  }

  ipc_request_reply(req);
}

static ssize_t
do_read_locked(struct FS *fs, struct Thread *sender,
               struct Channel *channel,
               uintptr_t va,
               size_t nbyte)
{
  ssize_t total;

  struct File *file = get_channel_file(channel);
  k_assert(file != NULL);

  if (!fs_inode_permission(sender, file->inode, FS_PERM_READ, 0))
    return -EPERM;

  if ((off_t) (file->offset + nbyte) < file->offset)
    return -EINVAL;

  if ((off_t) (file->offset + nbyte) > file->inode->size)
    nbyte = file->inode->size - file->offset;
  if (nbyte == 0)
    return 0;

  total = fs->ops->read(sender, file->inode, va, nbyte, file->offset);

  if (total >= 0) {
    file->offset += total;
  
    file->inode->atime  = time_get_seconds();
    file->inode->flags |= FS_INODE_DIRTY;
  }

  return total;
}

void
do_read(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  struct Channel *channel = req->channel;
  uintptr_t va = msg->u.read.va;
  size_t nbyte = msg->u.read.nbyte;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->rdev >= 0) {
    msg->r = dev_read(sender, file->rdev, va, nbyte);
  } else if ((channel->flags & O_ACCMODE) == O_WRONLY) {
    msg->r = -EBADF;
  } else if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    msg->r = do_read_locked(fs, req->sender, channel, va, nbyte);
    fs_inode_unlock(file->inode);
  }

  ipc_request_reply(req);
}

static ssize_t
do_readdir_locked(struct FS *fs, struct Thread *sender,
                  struct Channel *channel,
                  uintptr_t va,
                  size_t nbyte)
{
  ssize_t total = 0;

  struct {
    struct dirent de;
    char buf[NAME_MAX + 1];
  } de;

  struct File *file = get_channel_file(channel);
  k_assert(file != NULL);

  if (!S_ISDIR(file->inode->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(sender, file->inode, FS_PERM_READ, 0))
    return -EPERM;

  while (nbyte > 0) {
    ssize_t nread;
    int r;

    nread = fs->ops->readdir(sender, file->inode, &de, fs_filldir, file->offset);

    if (nread < 0)
      return nread;

    if (nread == 0)
      break;
    
    if (de.de.d_reclen > nbyte) {
      if (total == 0) {
        return -EINVAL;
      }
      break;
    }

    file->offset += nread;

    if ((r = vm_space_copy_out(sender, &de, va, de.de.d_reclen)) < 0)
      return r;

    va    += de.de.d_reclen;
    total += de.de.d_reclen;
    nbyte -= de.de.d_reclen;
  }

  return total;
}

void
do_readdir(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  struct Channel *channel = req->channel;
  uintptr_t va = msg->u.readdir.va;
  size_t nbyte = msg->u.readdir.nbyte;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    msg->r = do_readdir_locked(fs, sender, channel, va, nbyte);
    fs_inode_unlock(file->inode);
  }

  ipc_request_reply(req);
}

void
do_seek(struct FS *, struct IpcRequest *req, struct IpcMessage *msg)
{
  off_t offset = msg->u.seek.offset;
  int whence = msg->u.seek.whence;
  off_t new_offset;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    msg->r = -EINVAL;
    ipc_request_reply(req);
    return;
  }

  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = file->offset + offset;
    break;
  case SEEK_END:
    fs_inode_lock(file->inode);
    new_offset = file->inode->size + offset;
    fs_inode_unlock(file->inode);
    break;
  default:
    msg->r = -EINVAL;
    ipc_request_reply(req);
    return;
  }

  if (new_offset < 0) {
    msg->r = -EOVERFLOW;
  } else {
    file->offset = new_offset;
    msg->r = new_offset;
  }

  ipc_request_reply(req);
}

void
do_select(struct FS *, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct timeval *timeout = msg->u.select.timeout;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->rdev >= 0) {
    msg->r = dev_select(thread_current(), file->rdev, timeout);
    ipc_request_reply(req);
    return;
  }

  if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    // FIXME: select
    fs_inode_unlock(file->inode);
    msg->r = 1;
  }

  ipc_request_reply(req);
}

void
do_trunc(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  off_t length = msg->u.trunc.length;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (length < 0) {
    msg->r = -EINVAL;
  } else if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);

    if (length > file->inode->size) {
      fs_inode_unlock(file->inode);
      msg->r = -EFBIG;
    } else if (!fs_inode_permission(sender, file->inode, FS_PERM_WRITE, 0)) {
      fs_inode_unlock(file->inode);
      msg->r = -EPERM;
    } else {
      fs->ops->trunc(sender, file->inode, length);

      file->inode->size = length;
      file->inode->ctime = file->inode->mtime = time_get_seconds();

      file->inode->flags |= FS_INODE_DIRTY;

      fs_inode_unlock(file->inode);
      msg->r = 0;
    }
  }

  ipc_request_reply(req);
}

static ssize_t
do_write_locked(struct FS *fs, struct Thread *sender,
                struct Channel *channel,
                uintptr_t va,
                size_t nbyte)
{
  ssize_t total;

  struct File *file = get_channel_file(channel);
  k_assert(file != NULL);

  if (!fs_inode_permission(sender, file->inode, FS_PERM_WRITE, 0))
    return -EPERM;

  if (channel->flags & O_APPEND)
    file->offset = file->inode->size;

  if ((off_t) (file->offset + nbyte) < file->offset)
    return -EINVAL;

  if (nbyte == 0)
    return 0;

  total = fs->ops->write(sender, file->inode, va, nbyte, file->offset);

  if (total > 0) {
    file->offset += total;

    if (file->offset > file->inode->size)
      file->inode->size = file->offset;

    file->inode->mtime = time_get_seconds();
    file->inode->flags |= FS_INODE_DIRTY;
  }

  return total;
}

void
do_write(struct FS *fs, struct IpcRequest *req, struct IpcMessage *msg)
{
  struct Thread *sender = req->sender;
  struct Channel *channel = req->channel;
  uintptr_t va = msg->u.write.va;
  size_t nbyte = msg->u.write.nbyte;

  struct File *file = get_channel_file(req->channel);
  k_assert(file != NULL);

  if (file->rdev >= 0) {
    msg->r = dev_write(sender, file->rdev, va, nbyte);
  } else if (file->inode == NULL) {
    msg->r = -EINVAL;
  } else if ((channel->flags & O_ACCMODE) == O_RDONLY) {
    msg->r = -EBADF;
  } else {
    fs_inode_lock(file->inode);
    msg->r = do_write_locked(fs, sender, channel, va, nbyte);
    fs_inode_unlock(file->inode);
  }

  ipc_request_reply(req);
}

/*
 * ----- Message Handler Dispatch Table -----
 */

typedef void (*fs_handler_t)(struct FS *, struct IpcRequest *, struct IpcMessage *);

static fs_handler_t
fs_dispatch_table[] = {
  [IPC_MSG_ACCESS]   = do_access,
  [IPC_MSG_CHDIR]    = do_chdir,
  [IPC_MSG_CHMOD]    = do_chmod,
  [IPC_MSG_CHOWN]    = do_chown,
  [IPC_MSG_CREATE]   = do_create,
  [IPC_MSG_LINK]     = do_link,
  [IPC_MSG_LOOKUP]   = do_lookup,
  [IPC_MSG_STAT]     = do_stat,
  [IPC_MSG_READLINK] = do_readlink,
  [IPC_MSG_RMDIR]    = do_rmdir,
  [IPC_MSG_SYMLINK]  = do_symlink,
  [IPC_MSG_UNLINK]   = do_unlink,
  [IPC_MSG_UTIME]    = do_utime,
  [IPC_MSG_CLOSE]    = do_close,
  [IPC_MSG_FCHMOD]   = do_fchmod,
  [IPC_MSG_FCHOWN]   = do_fchown,
  [IPC_MSG_FSTAT]    = do_fstat,
  [IPC_MSG_FSYNC]    = do_fsync,
  [IPC_MSG_IOCTL]    = do_ioctl,
  [IPC_MSG_OPEN]     = do_open,
  [IPC_MSG_READ]     = do_read,
  [IPC_MSG_READDIR]  = do_readdir,
  [IPC_MSG_SEEK]     = do_seek,
  [IPC_MSG_SELECT]   = do_select,
  [IPC_MSG_TRUNC]    = do_trunc,
  [IPC_MSG_WRITE]    = do_write,
};

#define FS_DISPATCH_TABLE_SIZE (int)(sizeof(fs_dispatch_table) / sizeof(fs_dispatch_table[0]))

void
fs_service_task(void *arg)
{
  struct FS *fs = (struct FS *) arg;
  struct IpcRequest *req;

  while (k_mailbox_receive(&fs->mbox, (void *) &req) >= 0) {
    struct IpcMessage *msg = req->msg;

    if (msg == NULL) {
      ipc_request_reply(req);
      continue;
    }

    if (msg->type >= 0 && msg->type < FS_DISPATCH_TABLE_SIZE && fs_dispatch_table[msg->type] != NULL) {
      fs_dispatch_table[msg->type](fs, req, msg);
    } else {
      k_warn("unsupported msg type %d\n", msg->type);
      msg->r = -ENOSYS;
      ipc_request_reply(req);
    }
  }

  k_panic("error");
}

struct FS *
fs_create_service(char *name, dev_t dev, void *extra, struct FSOps *ops)
{
  struct FS *fs;
  int i;

  // TODO: move this code out from here
  if (k_list_is_null(&file_hash.table[0])) {
    HASH_INIT(file_hash.table);
    k_spinlock_init(&file_hash.lock, "file_hash");
  }

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
fs_send_recv(struct Channel *channel, struct IpcMessage *msg)
{
  struct IpcRequest *req;
  unsigned long long timeout = seconds2ticks(5);
  int r;

  k_assert(channel->type == CHANNEL_TYPE_FILE);

  if (channel->fs == NULL)
    return -1;

  if ((req = ipc_request_create(msg)) == NULL)
    return -ENOMEM;  

  req->channel = channel;
  req->sender = thread_current();

  ipc_request_dup(req);

  if (k_mailbox_timed_send(&channel->fs->mbox, &req, timeout) < 0) {
    ipc_request_destroy(req);
    ipc_request_destroy(req);
    k_warn("fail send %d: %d", msg->type, r);
    return -ETIMEDOUT;
  }

  if ((r = k_semaphore_timed_get(&req->sem, timeout, K_SLEEP_UNINTERUPTIBLE)) < 0) {
    ipc_request_destroy(req);
    ipc_request_destroy(req);
    k_warn("fail recv %d: %d", msg->type, r);
    return -ETIMEDOUT;
  }

  ipc_request_destroy(req);

  return 0;
}
