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

int
do_access(struct FS *fs, struct Thread *sender,
          ino_t ino,
          int amode)
{
  struct Inode *inode = fs->ops->inode_get(fs, ino);
  int r = 0;

  if (amode == F_OK)
    return r;

  fs_inode_lock(inode);

  if ((amode & R_OK) && !fs_inode_permission(sender, inode, FS_PERM_READ, 1))
    r = -EPERM;
  if ((amode & W_OK) && !fs_inode_permission(sender, inode, FS_PERM_WRITE, 1))
    r = -EPERM;
  if ((amode & X_OK) && !fs_inode_permission(sender, inode, FS_PERM_EXEC, 1))
    r = -EPERM;

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

int
do_chdir(struct FS *fs, struct Thread *sender,
         ino_t ino)
{
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);

  if (!S_ISDIR(inode->mode)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    return -ENOTDIR;
  }

  if (!fs_inode_permission(sender, inode, FS_PERM_EXEC, 0)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    return -EPERM;
  }

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return 0;
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

  inode->mode  = (inode->mode & ~CHMOD_MASK) | (mode & CHMOD_MASK);
  inode->ctime = time_get_seconds();
  inode->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(inode);

  return 0;
}

int
do_chmod(struct FS *fs, struct Thread *sender,
        ino_t ino,
        mode_t mode)
{
  int r;
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = do_inode_chmod(fs, sender, inode, mode);

  fs_inode_put(inode);

  return r;
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

int
do_chown(struct FS *fs, struct Thread *sender,
        ino_t ino,
        uid_t uid, gid_t gid)
{
  int r;
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = do_inode_chown(fs, sender, inode, uid, gid);

  fs_inode_put(inode);

  return r;
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

static int
do_create(struct FS *fs, struct Thread *sender,
          ino_t dir_ino,
          char *name,
          mode_t mode,
          dev_t dev,
          ino_t *istore)
{
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = NULL;

  fs_inode_lock(dir);
  r = do_create_locked(fs, sender, dir, name, mode, dev, &inode);
  fs_inode_unlock(dir);

  if (inode != NULL) {
    if (istore)
      *istore = inode->ino;
    fs_inode_put(inode);
  }

  fs_inode_put(dir);

  return r;
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

static int
do_link(struct FS *fs, struct Thread *sender,
        ino_t dir_ino,
        char *name,
        ino_t ino)
{
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);
  r = do_link_locked(fs, sender, dir, name, inode);
  fs_inode_unlock_two(dir, inode);

  fs_inode_put(dir);
  fs_inode_put(inode);

  return r;
}

static int
do_lookup(struct FS *fs, struct Thread *sender,
          ino_t dir_ino,
          const char *name,
          int flags,
          ino_t *istore)
{
  struct Inode *inode, *dir;

  dir = fs->ops->inode_get(fs, dir_ino);

  fs_inode_lock(dir);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock(dir);
    fs_inode_put(dir);
    return -ENOTDIR;
  }

  if (!fs_inode_permission(sender, dir, FS_PERM_READ, flags & FS_LOOKUP_REAL)) {
    fs_inode_unlock(dir);
    fs_inode_put(dir);
    return -EPERM;
  }
    
  inode = fs->ops->lookup(sender, dir, name);

  if (istore != NULL)
    *istore = inode ? inode->ino : 0;
  if (inode != NULL)
    fs_inode_put(inode);

  fs_inode_unlock(dir);
  fs_inode_put(dir);

  return 0;
}

static ssize_t
do_readlink(struct FS *fs, struct Thread *sender,
            ino_t ino,
            uintptr_t va,
            size_t nbyte)
{
  ssize_t r;
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);

  if (!fs_inode_permission(sender, inode, FS_PERM_READ, 0)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    return -EPERM;
  }
    
  if (!S_ISLNK(inode->mode)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    return -EINVAL;
  }

  r = fs->ops->readlink(sender, inode, va, nbyte);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

static int
do_rmdir(struct FS *fs, struct Thread *sender,
         ino_t dir_ino,
         ino_t ino,
         const char *name)
{
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    return -ENOTDIR;
  }

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    return -EPERM;
  }

  // TODO: Allow links to directories?
  if (!S_ISDIR(inode->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    return -EPERM;
  }

  r = fs->ops->rmdir(sender, dir, inode, name);

  fs_inode_unlock_two(dir, inode);
  fs_inode_put(dir);
  fs_inode_put(inode);

  return r;
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

int
do_stat(struct FS *fs, struct Thread *sender,
        ino_t ino,
        struct stat *buf)
{
  int r;
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = do_inode_stat(fs, sender, inode, buf);
  
  fs_inode_put(inode);

  return r;
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

static int
do_symlink(struct FS *fs, struct Thread *sender,
           ino_t dir_ino,
           char *name,
           mode_t mode,
           const char *link_path,
           ino_t *istore)
{
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = NULL;

  fs_inode_lock(dir);
  r = do_symlink_locked(fs, sender, dir, name, mode, link_path, &inode);
  fs_inode_unlock(dir);

  if (inode != NULL) {
    if (istore)
      *istore = inode->ino;
    fs_inode_put(inode);
  }

  fs_inode_put(dir);

  return r;
}

static int
do_unlink(struct FS *fs, struct Thread *sender,
          ino_t dir_ino,
          ino_t ino,
          const char *name)
{
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    return -ENOTDIR;
  }

  if (!fs_inode_permission(sender, dir, FS_PERM_WRITE, 0)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    return -EPERM;
  }

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    return -EPERM;
  }

  r = fs->ops->unlink(sender, dir, inode, name);

  fs_inode_unlock_two(dir, inode);
  fs_inode_put(dir);
  fs_inode_put(inode);

  return r;
}

int
do_utime(struct FS *fs, struct Thread *sender,
         ino_t ino, struct utimbuf *times)
{
  struct Inode *inode = fs->ops->inode_get(fs, ino);
  uid_t euid = sender ? sender->process->euid : 0;
  int r = 0;

  fs_inode_lock(inode);

  if (times == NULL) {
    if ((euid != 0) &&
        (euid != inode->uid) &&
        !fs_inode_permission(sender, inode, FS_PERM_WRITE, 1)) {
      r = -EPERM;
      goto out;
    }

    inode->atime = time_get_seconds();
    inode->mtime = time_get_seconds();

    inode->flags |= FS_INODE_DIRTY;
  } else {
    if ((euid != 0) ||
       ((euid != inode->uid) ||
         !fs_inode_permission(sender, inode, FS_PERM_WRITE, 1))) {
      r = -EPERM;
      goto out;
    }

    inode->atime = times->actime;
    inode->mtime = times->modtime;

    inode->flags |= FS_INODE_DIRTY;
  }

out:
  fs_inode_unlock(inode);
  fs_inode_put(inode);

  return r;
}

/*
 * ----- File Operations -----
 */

int
do_close(struct FS *, struct Thread *,
         struct Channel *channel)
{
  if (channel->u.file.inode != NULL) {
    fs_inode_put(channel->u.file.inode);
    channel->u.file.inode = NULL;
  }

  return 0;
}

static int
do_fchmod(struct FS *fs, struct Thread *sender,
          struct Channel *channel, mode_t mode)
{
  if (channel->u.file.inode == NULL)
    return -EINVAL;

  return do_inode_chmod(fs, sender, channel->u.file.inode, mode);
}

static int
do_fchown(struct FS *fs, struct Thread *sender,
          struct Channel *channel, uid_t uid, gid_t gid)
{
  if (channel->u.file.inode == NULL)
    return -EINVAL;

  return do_inode_chown(fs, sender, channel->u.file.inode, uid, gid);
}

static int
do_fstat(struct FS *fs, struct Thread *sender,
         struct Channel *channel, struct stat *buf)
{
  if (channel->u.file.inode == NULL)
    return -EINVAL;

  return do_inode_stat(fs, sender, channel->u.file.inode, buf);
}

int
do_fsync(struct FS *, struct Thread *,
         struct Channel *channel)
{
  if (channel->u.file.inode == NULL)
    return -EINVAL;

  fs_inode_lock(channel->u.file.inode);
  // FIXME: fsync
  fs_inode_unlock(channel->u.file.inode);

  return 0;
}

static int
do_ioctl(struct FS *, struct Thread *sender,
         struct Channel *channel, int request, int arg)
{
  if (channel->u.file.rdev >= 0)
    return dev_ioctl(sender, channel->u.file.rdev, request, arg);

  if (channel->u.file.inode == NULL)
    return -EINVAL;

  fs_inode_lock(channel->u.file.inode);
  // FIXME: ioctl
  fs_inode_unlock(channel->u.file.inode);

  return -ENOTTY;
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

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode))
    channel->u.file.rdev = inode->rdev;

  if (oflag & O_APPEND)
    channel->u.file.offset = inode->size;

  channel->u.file.inode = fs_inode_duplicate(inode);

  return 0;
}

static int
do_open(struct FS *fs, struct Thread *sender,
        struct Channel *channel, 
        ino_t ino,
        int oflag,
        mode_t mode)
{
  int r;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);
  r = do_open_locked(fs, sender, channel, inode, oflag);
  fs_inode_unlock(inode);

  fs_inode_put(inode);

  if ((r == 0) && (channel->u.file.rdev >= 0)){
    r = dev_open(sender, channel->u.file.rdev, oflag, mode);
  }

  return r;
}

static ssize_t
do_read_locked(struct FS *fs, struct Thread *sender,
               struct Channel *channel,
               uintptr_t va,
               size_t nbyte)
{
  ssize_t total;

  if (!fs_inode_permission(sender, channel->u.file.inode, FS_PERM_READ, 0))
    return -EPERM;

  if ((off_t) (channel->u.file.offset + nbyte) < channel->u.file.offset)
    return -EINVAL;

  if ((off_t) (channel->u.file.offset + nbyte) > channel->u.file.inode->size)
    nbyte = channel->u.file.inode->size - channel->u.file.offset;
  if (nbyte == 0)
    return 0;

  total = fs->ops->read(sender, channel->u.file.inode, va, nbyte, channel->u.file.offset);

  if (total >= 0) {
    channel->u.file.offset += total;
  
    channel->u.file.inode->atime  = time_get_seconds();
    channel->u.file.inode->flags |= FS_INODE_DIRTY;
  }

  return total;
}

static ssize_t
do_read(struct FS *fs, struct Thread *sender,
        struct Channel *channel,
        uintptr_t va,
        size_t nbyte)
{
  ssize_t r;

  if (channel->u.file.rdev >= 0)
    return dev_read(sender, channel->u.file.rdev, va, nbyte);

  if ((channel->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  if (channel->u.file.inode == NULL)
    return -EINVAL;

  fs_inode_lock(channel->u.file.inode);
  r = do_read_locked(fs, sender, channel, va, nbyte);
  fs_inode_unlock(channel->u.file.inode);

  return r;
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

  if (!S_ISDIR(channel->u.file.inode->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(sender, channel->u.file.inode, FS_PERM_READ, 0))
    return -EPERM;

  while (nbyte > 0) {
    ssize_t nread;
    int r;

    nread = fs->ops->readdir(sender, channel->u.file.inode, &de, fs_filldir, channel->u.file.offset);

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

    channel->u.file.offset += nread;

    if ((r = vm_space_copy_out(sender, &de, va, de.de.d_reclen)) < 0)
      return r;

    va    += de.de.d_reclen;
    total += de.de.d_reclen;
    nbyte -= de.de.d_reclen;
  }

  return total;
}

static ssize_t
do_readdir(struct FS *fs, struct Thread *sender,
           struct Channel *channel,
           uintptr_t va,
           size_t nbyte)
{
  ssize_t r;

  if (channel->u.file.inode == NULL)
    return -EINVAL;

  fs_inode_lock(channel->u.file.inode);
  r = do_readdir_locked(fs, sender, channel, va, nbyte);
  fs_inode_unlock(channel->u.file.inode);

  return r;
}

static off_t
do_seek(struct FS *, struct Thread *,
        struct Channel *channel,
        off_t offset,
        int whence)
{
  off_t new_offset;

  if (channel->u.file.inode == NULL)
    return -EINVAL;

  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = channel->u.file.offset + offset;
    break;
  case SEEK_END:
    fs_inode_lock(channel->u.file.inode);
    new_offset = channel->u.file.inode->size + offset;
    fs_inode_unlock(channel->u.file.inode);
    break;
  default:
    return -EINVAL;
  }

  if (new_offset < 0)
    return -EOVERFLOW;

  channel->u.file.offset = new_offset;

  return new_offset;
}

static int
do_select(struct FS *, struct Thread *,
          struct Channel *channel,
          struct timeval *)
{
  if (channel->u.file.inode == NULL)
    return -EINVAL;

  fs_inode_lock(channel->u.file.inode);
  // FIXME: select
  fs_inode_unlock(channel->u.file.inode);

  return 1;
}

static int
do_trunc(struct FS *fs, struct Thread *sender,
         struct Channel *channel,
         off_t length)
{
  if (length < 0)
    return -EINVAL;

  if (channel->u.file.inode == NULL)
    return -EINVAL;

  fs_inode_lock(channel->u.file.inode);

  if (length > channel->u.file.inode->size) {
    fs_inode_unlock(channel->u.file.inode);
    return -EFBIG;
  }

  if (!fs_inode_permission(sender, channel->u.file.inode, FS_PERM_WRITE, 0)) {
    fs_inode_unlock(channel->u.file.inode);
    return -EPERM;
  }

  fs->ops->trunc(sender, channel->u.file.inode, length);

  channel->u.file.inode->size = length;
  channel->u.file.inode->ctime = channel->u.file.inode->mtime = time_get_seconds();

  channel->u.file.inode->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(channel->u.file.inode);

  return 0;
}

static ssize_t
do_write_locked(struct FS *fs, struct Thread *sender,
                struct Channel *channel,
                uintptr_t va,
                size_t nbyte)
{
  ssize_t total;

  if (!fs_inode_permission(sender, channel->u.file.inode, FS_PERM_WRITE, 0))
    return -EPERM;

  if (channel->flags & O_APPEND)
    channel->u.file.offset = channel->u.file.inode->size;

  if ((off_t) (channel->u.file.offset + nbyte) < channel->u.file.offset)
    return -EINVAL;

  if (nbyte == 0)
    return 0;

  total = fs->ops->write(sender, channel->u.file.inode, va, nbyte, channel->u.file.offset);

  if (total > 0) {
    channel->u.file.offset += total;

    if (channel->u.file.offset > channel->u.file.inode->size)
      channel->u.file.inode->size = channel->u.file.offset;

    channel->u.file.inode->mtime = time_get_seconds();
    channel->u.file.inode->flags |= FS_INODE_DIRTY;
  }

  return total;
}

static ssize_t
do_write(struct FS *fs, struct Thread *sender,
         struct Channel *channel,
         uintptr_t va,
         size_t nbyte)
{
  ssize_t r;

  if (channel->u.file.rdev >= 0)
    return dev_write(sender, channel->u.file.rdev, va, nbyte);

  if (channel->u.file.inode == NULL)
    return -EINVAL;

  if ((channel->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  fs_inode_lock(channel->u.file.inode);
  r = do_write_locked(fs, sender, channel, va, nbyte);
  fs_inode_unlock(channel->u.file.inode);

  return r;
}

void
fs_service_task(void *arg)
{
  struct FS *fs = (struct FS *) arg;
  struct IpcMessage *msg;

  while (k_mailbox_receive(&fs->mbox, (void *) &msg) >= 0) {
    k_assert(msg != NULL);

    switch (msg->type) {
    case IPC_MSG_ACCESS:
      msg->u.access.r = do_access(fs, msg->sender,
                                  msg->u.access.ino,
                                  msg->u.access.amode);
      break;
    case IPC_MSG_CHDIR:
      msg->u.chdir.r = do_chdir(fs, msg->sender,
                                msg->u.chdir.ino);
      break;
    case IPC_MSG_CHMOD:
      msg->u.chmod.r = do_chmod(fs, msg->sender,
                                msg->u.chmod.ino,
                                msg->u.chmod.mode);
      break;
    case IPC_MSG_CHOWN:
      msg->u.chown.r = do_chown(fs, msg->sender,
                                msg->u.chown.ino,
                                msg->u.chown.uid,
                                msg->u.chown.gid);
      break;
    case IPC_MSG_CREATE:
      msg->u.create.r = do_create(fs, msg->sender,
                                  msg->u.create.dir_ino,
                                  msg->u.create.name,
                                  msg->u.create.mode,
                                  msg->u.create.dev,
                                  msg->u.create.istore);
      break;
    case IPC_MSG_LINK:
      msg->u.link.r = do_link(fs, msg->sender,
                              msg->u.link.dir_ino,
                              msg->u.link.name,
                              msg->u.link.ino);
      break;
    case IPC_MSG_LOOKUP:
      msg->u.lookup.r = do_lookup(fs, msg->sender,
                                  msg->u.lookup.dir_ino,
                                  msg->u.lookup.name,
                                  msg->u.lookup.flags,
                                  msg->u.lookup.istore);
      break;
    case IPC_MSG_READLINK:
      msg->u.readlink.r = do_readlink(fs, msg->sender,
                                      msg->u.readlink.ino,
                                      msg->u.readlink.va,
                                      msg->u.readlink.nbyte);
      break;
    case IPC_MSG_RMDIR:
      msg->u.rmdir.r = do_rmdir(fs, msg->sender,
                                msg->u.rmdir.dir_ino,
                                msg->u.rmdir.ino,
                                msg->u.rmdir.name);
      break;
    case IPC_MSG_STAT:
      msg->u.stat.r = do_stat(fs, msg->sender,
                              msg->u.stat.ino,
                              msg->u.stat.buf);
      break;
    case IPC_MSG_SYMLINK:
      msg->u.symlink.r = do_symlink(fs, msg->sender,
                                    msg->u.symlink.dir_ino,
                                    msg->u.symlink.name,
                                    msg->u.symlink.mode,
                                    msg->u.symlink.path,
                                    msg->u.symlink.istore);
      break;
    case IPC_MSG_UNLINK:
      msg->u.unlink.r = do_unlink(fs, msg->sender,
                                  msg->u.unlink.dir_ino,
                                  msg->u.unlink.ino,
                                  msg->u.unlink.name);
      break;
    case IPC_MSG_UTIME:
      msg->u.utime.r = do_utime(fs, msg->sender,
                                msg->u.utime.ino,
                                msg->u.utime.times);
      break;

    case IPC_MSG_CLOSE:
      msg->u.close.r = do_close(fs, msg->sender,
                                msg->channel);
      break;
    case IPC_MSG_FCHMOD:
      msg->u.fchmod.r = do_fchmod(fs, msg->sender,
                                  msg->channel,
                                  msg->u.fchmod.mode);
      break;
    case IPC_MSG_FCHOWN:
      msg->u.fchown.r = do_fchown(fs, msg->sender,
                                  msg->channel,
                                  msg->u.fchown.uid,
                                  msg->u.fchown.gid);
      break;
    case IPC_MSG_FSTAT:
      msg->u.fstat.r = do_fstat(fs, msg->sender,
                                msg->channel,
                                msg->u.fstat.buf);
      break;
    case IPC_MSG_FSYNC:
      msg->u.fsync.r = do_fsync(fs, msg->sender,
                                msg->channel);
      break;
    case IPC_MSG_IOCTL:
      msg->u.ioctl.r = do_ioctl(fs, msg->sender,
                                msg->channel,
                                msg->u.ioctl.request,
                                msg->u.ioctl.arg);
      break;
    case IPC_MSG_OPEN:
      msg->u.open.r = do_open(fs, msg->sender,
                              msg->channel,
                              msg->u.open.ino,
                              msg->u.open.oflag,
                              msg->u.open.mode);
      break;
    case IPC_MSG_READ:
      msg->u.read.r = do_read(fs, msg->sender,
                              msg->channel,
                              msg->u.read.va,
                              msg->u.read.nbyte);
      break;
    case IPC_MSG_READDIR:
      msg->u.readdir.r = do_readdir(fs, msg->sender,
                                    msg->channel,
                                    msg->u.readdir.va,
                                    msg->u.readdir.nbyte);
      break;
    case IPC_MSG_SEEK:
      msg->u.seek.r = do_seek(fs, msg->sender,
                              msg->channel,
                              msg->u.seek.offset,
                              msg->u.seek.whence);
      break;
    case IPC_MSG_SELECT:
      msg->u.select.r = do_select(fs, msg->sender,
                                  msg->channel,
                                  msg->u.select.timeout);
      break;
    case IPC_MSG_TRUNC:
      msg->u.trunc.r = do_trunc(fs, msg->sender,
                                msg->channel,
                                msg->u.trunc.length);
      break;
    case IPC_MSG_WRITE:
      msg->u.write.r = do_write(fs, msg->sender,
                                msg->channel,
                                msg->u.write.va,
                                msg->u.write.nbyte);
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
fs_send_recv(struct Channel *channel, struct IpcMessage *msg)
{
  struct IpcMessage *msg_ptr = msg;
  unsigned long long timeout = seconds2ticks(5);
  int r;

  k_assert(channel->type == CHANNEL_TYPE_FILE);

  if (channel->fs == NULL)
    return -1;

  k_semaphore_create(&msg->sem, 0);

  msg->sender = thread_current();
  msg->channel = channel;

  if (k_mailbox_timed_send(&channel->fs->mbox, &msg_ptr, timeout) < 0)
    k_panic("fail send %d: %d", msg->type, r);

  if ((r = k_semaphore_timed_get(&msg->sem, timeout, K_SLEEP_UNINTERUPTIBLE)) < 0)
    k_panic("fail recv %d: %d", msg->type, r);

  return 0;
}
