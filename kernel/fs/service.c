#include <dirent.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/ipc.h>
#include <kernel/page.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/time.h>
#include <kernel/vmspace.h>
#include <kernel/dev.h>
#include <kernel/hash.h>

// File data

#define NBUCKET   256

static struct {
  struct KListLink table[NBUCKET];
  struct KSpinLock lock;
} file_hash;

static struct File *
get_connection_file(struct Connection *connection)
{
  struct KListLink *l;

  k_spinlock_acquire(&file_hash.lock);

  HASH_FOREACH_ENTRY(file_hash.table, l, (uintptr_t) connection) {
    struct File *file;
    
    file = KLIST_CONTAINER(l, struct File, hash_link);
    if (file->connection == connection) {
      k_spinlock_release(&file_hash.lock);
      return file;
    }
  }

  k_spinlock_release(&file_hash.lock);
  return NULL;
}

static void
set_connection_file(struct File *file,
                 struct Connection *connection)
{
  file->connection = connection;
  k_list_null(&file->hash_link);

  k_spinlock_acquire(&file_hash.lock);
  HASH_PUT(file_hash.table, &file->hash_link, (uintptr_t) connection);
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
do_access(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  int r;

  mode_t amode = msg->u.access.amode;
  ino_t ino = msg->u.access.ino;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = 0;

  if (msg->u.access.amode == F_OK) {
    fs_inode_put(inode);
    request_reply(req, r);
    return;
  }

  fs_inode_lock(inode);

  if ((amode & R_OK) && !fs_inode_permission(process, inode, FS_PERM_READ, 1))
    r = -EPERM;
  if ((amode & W_OK) && !fs_inode_permission(process, inode, FS_PERM_WRITE, 1))
    r = -EPERM;
  if ((amode & X_OK) && !fs_inode_permission(process, inode, FS_PERM_EXEC, 1))
    r = -EPERM;

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  request_reply(req, r);
}

void
do_chdir(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  ino_t ino = msg->u.chdir.ino;
  int r;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);

  if (!S_ISDIR(inode->mode)) {
    r = -ENOTDIR;
  } else if (!fs_inode_permission(process, inode, FS_PERM_EXEC, 0)) {
    r = -EPERM;
  } else {
    r = 0;
  }

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  request_reply(req, r);
}

#define CHMOD_MASK  (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)

int
do_inode_chmod(struct FS *, struct Process *process,
               struct Inode *inode, mode_t mode)
{
  uid_t euid = process ? process->euid : 0;

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
do_chmod(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  ino_t ino = msg->u.chmod.ino;
  mode_t mode = msg->u.chmod.mode;
  int r;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = do_inode_chmod(fs, req->process, inode, mode);

  fs_inode_put(inode);

  request_reply(req, r);
}

static int
do_chown_locked(struct FS *, struct Process *process,
                struct Inode *inode,
                uid_t uid,
                gid_t gid)
{
  uid_t euid = process ? process->euid : 0;
  uid_t egid = process ? process->egid : 0;

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
do_inode_chown(struct FS *fs, struct Process *process,
               struct Inode *inode,
               uid_t uid, gid_t gid)
{
  int r;

  fs_inode_lock(inode);
  r = do_chown_locked(fs, process, inode, uid, gid);
  fs_inode_unlock(inode);

  return r;
}

void
do_chown(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  ino_t ino = msg->u.chown.ino;
  uid_t uid = msg->u.chown.uid;
  gid_t gid = msg->u.chown.gid;
  int r;

  // TODO: verify existence
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = do_inode_chown(fs, req->process, inode, uid, gid);

  fs_inode_put(inode);

  request_reply(req, r);
}

static int
do_create_locked(struct FS *fs, struct Process *process,
                 struct Inode *dir,
                 char *name,
                 mode_t mode,
                 dev_t dev,
                 struct Inode **istore)
{
  struct Inode *inode;

  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(process, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  inode = fs->ops->lookup(process, dir, name);

  if (inode != NULL) {
    fs_inode_put(inode);
    return -EEXIST;
  }

  switch (mode & S_IFMT) {
    case S_IFDIR:
      return fs->ops->mkdir(process, dir, name, mode, istore);
    case S_IFREG:
      return fs->ops->create(process, dir, name, mode, istore);
    default:
      return fs->ops->mknod(process, dir, name, mode, dev, istore);
  }
}

void
do_create(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  ino_t dir_ino = msg->u.create.dir_ino;
  char *name = msg->u.create.name;
  mode_t mode = msg->u.create.mode;
  dev_t dev  = msg->u.create.dev;
  ino_t *istore  = msg->u.create.istore;
  int r;

  // TODO: verify existence
  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = NULL;

  fs_inode_lock(dir);
  r = do_create_locked(fs, req->process, dir, name, mode, dev, &inode);
  fs_inode_unlock(dir);

  if (inode != NULL) {
    if (istore)
      *istore = inode->ino;
    fs_inode_put(inode);
  }

  fs_inode_put(dir);

  request_reply(req, r);
}

static int
do_link_locked(struct FS *fs, struct Process *process,
               struct Inode *dir,
               char *name,
               struct Inode *inode)
{
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(process, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;

  if (inode->nlink >= LINK_MAX)
    return -EMLINK;

  if (dir->dev != inode->dev)
    return -EXDEV;

  return fs->ops->link(process, dir, name, inode);
}

void
do_link(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  ino_t dir_ino = msg->u.link.dir_ino;
  char *name = msg->u.link.name;
  ino_t ino = msg->u.link.ino;
  int r;

  // TODO: verify existence
  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);
  r = do_link_locked(fs, req->process, dir, name, inode);
  fs_inode_unlock_two(dir, inode);

  fs_inode_put(dir);
  fs_inode_put(inode);

  request_reply(req, r);
}

void
do_lookup(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;

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
    request_reply(req, -ENOTDIR);
    return;
  }

  if (!fs_inode_permission(process, dir, FS_PERM_READ, flags & FS_LOOKUP_REAL)) {
    fs_inode_unlock(dir);
    fs_inode_put(dir);
    request_reply(req, -EPERM);
    return;
  }
    
  inode = fs->ops->lookup(process, dir, name);

  if (istore != NULL)
    *istore = inode ? inode->ino : 0;
  if (inode != NULL)
    fs_inode_put(inode);

  fs_inode_unlock(dir);
  fs_inode_put(dir);

  request_reply(req, 0);
}

void
do_readlink(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  ino_t ino = msg->u.readlink.ino;
  size_t nbyte = msg->u.readlink.nbyte;
  ssize_t r;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);

  if (!fs_inode_permission(process, inode, FS_PERM_READ, 0)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    request_reply(req, -EPERM);
    return;
  }
    
  if (!S_ISLNK(inode->mode)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    request_reply(req, -EINVAL);
    return;
  }

  r = fs->ops->readlink(req, inode, nbyte);

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  request_reply(req, r);
}

void
do_rmdir(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  ino_t dir_ino = msg->u.rmdir.dir_ino;
  ino_t ino = msg->u.rmdir.ino;
  const char *name = msg->u.rmdir.name;
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock_two(dir, inode);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    request_reply(req, -ENOTDIR);
    return;
  }

  if (!fs_inode_permission(process, dir, FS_PERM_WRITE, 0)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    request_reply(req, -EPERM);
    return;
  }

  // TODO: Allow links to directories?
  if (!S_ISDIR(inode->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    request_reply(req, -EPERM);
    return;
  }

  r = fs->ops->rmdir(process, dir, inode, name);

  fs_inode_unlock_two(dir, inode);
  fs_inode_put(dir);
  fs_inode_put(inode);

  request_reply(req, r);
}

int
do_inode_stat(struct FS *, struct Request *req,
              struct Inode *inode)
{
  struct stat buf;
  int r;

  memset(&buf, 0, sizeof(buf));
  
  fs_inode_lock(inode);
  
  buf.st_dev          = inode->dev;
  buf.st_ino          = inode->ino;
  buf.st_mode         = inode->mode;
  buf.st_nlink        = inode->nlink;
  buf.st_uid          = inode->uid;
  buf.st_gid          = inode->gid;
  buf.st_rdev         = inode->rdev;
  buf.st_size         = inode->size;
  buf.st_atim.tv_sec  = inode->atime;
  buf.st_atim.tv_nsec = 0;
  buf.st_mtim.tv_sec  = inode->mtime;
  buf.st_mtim.tv_nsec = 0;
  buf.st_ctim.tv_sec  = inode->ctime;
  buf.st_ctim.tv_nsec = 0;
  buf.st_blocks       = inode->size / S_BLKSIZE;
  buf.st_blksize      = S_BLKSIZE;

  fs_inode_unlock(inode);

  if ((r = request_write(req, &buf, sizeof(buf))) < 0)
    return r;

  return 0;
}

void
do_stat(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  ino_t ino = msg->u.stat.ino;
  int r;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  r = do_inode_stat(fs, req, inode);
  
  fs_inode_put(inode);

  request_reply(req, r);
}

static int
do_symlink_locked(struct FS *fs, struct Process *process,
                  struct Inode *dir,
                  char *name,
                  mode_t mode,
                  const char *path,
                  struct Inode **istore)
{
  struct Inode *inode;

  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(process, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  inode = fs->ops->lookup(process, dir, name);

  if (inode != NULL) {
    fs_inode_put(inode);
    return -EEXIST;
  }

  return fs->ops->symlink(process, dir, name, S_IFLNK | mode, path, istore);
}

void
do_symlink(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  ino_t dir_ino = msg->u.symlink.dir_ino;
  char *name = msg->u.symlink.name;
  mode_t mode = msg->u.symlink.mode;
  const char *link_path = msg->u.symlink.path;
  ino_t *istore = msg->u.symlink.istore;
  int r;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = NULL;

  fs_inode_lock(dir);
  r = do_symlink_locked(fs, process, dir, name, mode, link_path, &inode);
  fs_inode_unlock(dir);

  if (inode != NULL) {
    if (istore)
      *istore = inode->ino;
    fs_inode_put(inode);
  }

  fs_inode_put(dir);

  request_reply(req, r);
}

void
do_unlink(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  ino_t dir_ino = msg->u.unlink.dir_ino;
  ino_t ino = msg->u.unlink.ino;
  const char *name = msg->u.unlink.name;

  struct Inode *dir = fs->ops->inode_get(fs, dir_ino);
  struct Inode *inode = fs->ops->inode_get(fs, ino);

  int r;

  fs_inode_lock_two(dir, inode);

  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    request_reply(req, -ENOTDIR);
    return;
  }

  if (!fs_inode_permission(process, dir, FS_PERM_WRITE, 0)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    request_reply(req, -EPERM);
    return;
  }

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode)) {
    fs_inode_unlock_two(dir, inode);
    fs_inode_put(dir);
    fs_inode_put(inode);
    request_reply(req, -EPERM);
    return;
  }

  r = fs->ops->unlink(process, dir, inode, name);

  fs_inode_unlock_two(dir, inode);
  fs_inode_put(dir);
  fs_inode_put(inode);

  request_reply(req, r);
}

void
do_utime(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  ino_t ino = msg->u.utime.ino;
  struct utimbuf *times = msg->u.utime.times;
  int r;

  struct Inode *inode = fs->ops->inode_get(fs, ino);
  uid_t euid = process ? process->euid : 0;

  fs_inode_lock(inode);

  if (times == NULL) {
    if ((euid != 0) &&
        (euid != inode->uid) &&
        !fs_inode_permission(process, inode, FS_PERM_WRITE, 1)) {
      r = -EPERM;
      goto out;
    }

    inode->atime = time_get_seconds();
    inode->mtime = time_get_seconds();

    inode->flags |= FS_INODE_DIRTY;
    r = 0;
  } else {
    if ((euid != 0) ||
       ((euid != inode->uid) ||
         !fs_inode_permission(process, inode, FS_PERM_WRITE, 1))) {
      r = -EPERM;
      goto out;
    }

    inode->atime = times->actime;
    inode->mtime = times->modtime;

    inode->flags |= FS_INODE_DIRTY;
    r = 0;
  }

out:
  fs_inode_unlock(inode);
  fs_inode_put(inode);

  request_reply(req, r);
}

/*
 * ----- File Operations -----
 */

void
do_close(struct FS *, struct Request *req, struct IpcMessage *)
{
  struct File *file = get_connection_file(req->connection);

  if (file != NULL) {
    k_assert(file->inode != NULL);

    fs_inode_put(file->inode);

    k_spinlock_acquire(&file_hash.lock);
    k_list_remove(&file->hash_link);
    k_spinlock_release(&file_hash.lock);

    k_free(file);
  }

  request_reply(req, 0);
}

void
do_fchmod(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  mode_t mode = msg->u.fchmod.mode;
  int r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    r = do_inode_chmod(fs, process, file->inode, mode);
  }

  request_reply(req, r);
}

void
do_fchown(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  uid_t uid = msg->u.fchown.uid;
  gid_t gid = msg->u.fchown.gid;
  int r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    r = do_inode_chown(fs, process, file->inode, uid, gid);
  }

  request_reply(req, r);
}

void
do_fstat(struct FS *fs, struct Request *req, struct IpcMessage *)
{
  struct File *file = get_connection_file(req->connection);
  int r;

  k_assert(file != NULL);

  if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    r = do_inode_stat(fs, req, file->inode);
  }

  request_reply(req, r);
}

void
do_fsync(struct FS *, struct Request *req, struct IpcMessage *)
{
  struct File *file = get_connection_file(req->connection);
  int r;
  k_assert(file != NULL);

  if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    // FIXME: fsync
    fs_inode_unlock(file->inode);
    r = 0;
  }

  request_reply(req, r);
}

void
do_ioctl(struct FS *, struct Request *req, struct IpcMessage *msg)
{
  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);
  int request = msg->u.ioctl.request;
  int arg = msg->u.ioctl.arg;
  int r;

  if (file->rdev >= 0) {
    r = dev_ioctl(req, file->rdev, request, arg);
  } else if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    // FIXME: ioctl
    fs_inode_unlock(file->inode);
    r = -ENOTTY;
  }

  request_reply(req, r);
}

static int
do_open_locked(struct FS *fs, struct Process *process,
               struct Connection *connection, 
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
    fs->ops->trunc(process, inode, 0);

    inode->size = 0;
    inode->ctime = inode->mtime = time_get_seconds();
    inode->flags |= FS_INODE_DIRTY;
  }

  struct File *file = (struct File *) k_malloc(sizeof(struct File));
  k_assert(file != NULL);

  set_connection_file(file, connection);

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
do_open(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  struct Connection *connection = req->connection;
  ino_t ino = msg->u.open.ino;
  int oflag = msg->u.open.oflag;
  mode_t mode = msg->u.open.mode;
  int r;

  struct Inode *inode = fs->ops->inode_get(fs, ino);

  fs_inode_lock(inode);
  r = do_open_locked(fs, process, connection, inode, oflag);
  fs_inode_unlock(inode);

  fs_inode_put(inode);

  if (r == 0) {
    struct File *file = get_connection_file(req->connection);
    k_assert(file != NULL);

    if (file->rdev >= 0) {
      r = dev_open(req, file->rdev, oflag, mode);
    }
  }

  request_reply(req, r);
}

static ssize_t
do_read_locked(struct FS *fs, struct Request *req,
               struct Connection *connection,
               size_t nbyte)
{
  ssize_t total;

  struct File *file = get_connection_file(connection);
  k_assert(file != NULL);

  if (!fs_inode_permission(req->process, file->inode, FS_PERM_READ, 0))
    return -EPERM;

  if ((off_t) (file->offset + nbyte) < file->offset)
    return -EINVAL;

  if ((off_t) (file->offset + nbyte) > file->inode->size)
    nbyte = file->inode->size - file->offset;
  if (nbyte == 0)
    return 0;

  total = fs->ops->read(req, file->inode, nbyte, file->offset);

  if (total >= 0) {
    file->offset += total;
  
    file->inode->atime  = time_get_seconds();
    file->inode->flags |= FS_INODE_DIRTY;
  }

  return total;
}

void
do_read(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Connection *connection = req->connection;
  uintptr_t va = msg->u.read.va;
  size_t nbyte = msg->u.read.nbyte;
  ssize_t r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->rdev >= 0) {
    r = dev_read(req, file->rdev, va, nbyte);
  } else if ((connection->flags & O_ACCMODE) == O_WRONLY) {
    r = -EBADF;
  } else if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    r = do_read_locked(fs, req, connection, nbyte);
    fs_inode_unlock(file->inode);
  }

  request_reply(req, r);
}

static ssize_t
do_readdir_locked(struct FS *fs, struct Request *req,
                  struct Connection *connection,
                  uintptr_t va,
                  size_t nbyte)
{
  ssize_t total = 0;

  struct {
    struct dirent de;
    char buf[NAME_MAX + 1];
  } de;

  struct File *file = get_connection_file(connection);
  k_assert(file != NULL);

  if (!S_ISDIR(file->inode->mode))
    return -ENOTDIR;

  if (!fs_inode_permission(req->process, file->inode, FS_PERM_READ, 0))
    return -EPERM;

  while (nbyte > 0) {
    ssize_t nread;
    int r;

    nread = fs->ops->readdir(req->process, file->inode, &de, fs_filldir, file->offset);

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

    if ((r = request_write(req, &de, de.de.d_reclen)) < 0)
      return r;

    va    += de.de.d_reclen;
    total += de.de.d_reclen;
    nbyte -= de.de.d_reclen;
  }

  return total;
}

void
do_readdir(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Connection *connection = req->connection;
  uintptr_t va = msg->u.readdir.va;
  size_t nbyte = msg->u.readdir.nbyte;
  ssize_t r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    r = do_readdir_locked(fs, req, connection, va, nbyte);
    fs_inode_unlock(file->inode);
  }

  request_reply(req, r);
}

void
do_seek(struct FS *, struct Request *req, struct IpcMessage *msg)
{
  off_t offset = msg->u.seek.offset;
  int whence = msg->u.seek.whence;
  off_t new_offset;
  int r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->inode == NULL) {
    r = -EINVAL;
    request_reply(req, r);
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
    r = -EINVAL;
    request_reply(req, r);
    return;
  }

  if (new_offset < 0) {
    r = -EOVERFLOW;
  } else {
    file->offset = new_offset;
    r = new_offset;
  }

  request_reply(req, r);
}

void
do_select(struct FS *, struct Request *req, struct IpcMessage *msg)
{
  struct timeval *timeout = msg->u.select.timeout;
  int r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->rdev >= 0) {
    r = dev_select(req, file->rdev, timeout);
    request_reply(req, r);
    return;
  }

  if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);
    // FIXME: select
    fs_inode_unlock(file->inode);
    r = 1;
  }

  request_reply(req, r);
}

void
do_trunc(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  off_t length = msg->u.trunc.length;
  int r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (length < 0) {
    r = -EINVAL;
  } else if (file->inode == NULL) {
    r = -EINVAL;
  } else {
    fs_inode_lock(file->inode);

    if (length > file->inode->size) {
      fs_inode_unlock(file->inode);
      r = -EFBIG;
    } else if (!fs_inode_permission(process, file->inode, FS_PERM_WRITE, 0)) {
      fs_inode_unlock(file->inode);
      r = -EPERM;
    } else {
      fs->ops->trunc(process, file->inode, length);

      file->inode->size = length;
      file->inode->ctime = file->inode->mtime = time_get_seconds();

      file->inode->flags |= FS_INODE_DIRTY;

      fs_inode_unlock(file->inode);
      r = 0;
    }
  }

  request_reply(req, r);
}

static ssize_t
do_write_locked(struct FS *fs, struct Process *process,
                struct Connection *connection,
                uintptr_t va,
                size_t nbyte)
{
  ssize_t total;

  struct File *file = get_connection_file(connection);
  k_assert(file != NULL);

  if (!fs_inode_permission(process, file->inode, FS_PERM_WRITE, 0))
    return -EPERM;

  if (connection->flags & O_APPEND)
    file->offset = file->inode->size;

  if ((off_t) (file->offset + nbyte) < file->offset)
    return -EINVAL;

  if (nbyte == 0)
    return 0;

  total = fs->ops->write(process, file->inode, va, nbyte, file->offset);

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
do_write(struct FS *fs, struct Request *req, struct IpcMessage *msg)
{
  struct Process *process = req->process;
  struct Connection *connection = req->connection;
  uintptr_t va = msg->u.write.va;
  size_t nbyte = msg->u.write.nbyte;
  ssize_t r;

  struct File *file = get_connection_file(req->connection);
  k_assert(file != NULL);

  if (file->rdev >= 0) {
    r = dev_write(req, file->rdev, va, nbyte);
  } else if (file->inode == NULL) {
    r = -EINVAL;
  } else if ((connection->flags & O_ACCMODE) == O_RDONLY) {
    r = -EBADF;
  } else {
    fs_inode_lock(file->inode);
    r = do_write_locked(fs, process, connection, va, nbyte);
    fs_inode_unlock(file->inode);
  }

  request_reply(req, r);
}

/*
 * ----- Message Handler Dispatch Table -----
 */

typedef void (*fs_handler_t)(struct FS *, struct Request *, struct IpcMessage *);

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
  struct Request *req;

  while (endpoint_receive(&fs->endpoint, &req) >= 0) {
    struct IpcMessage msg;

    if (request_read(req, &msg, sizeof(msg)) < 0) {
      request_reply(req, -EFAULT);
    } else if (msg.type >= 0 && msg.type < FS_DISPATCH_TABLE_SIZE && fs_dispatch_table[msg.type] != NULL) {
      fs_dispatch_table[msg.type](fs, req, &msg);
    } else {
      k_warn("unsupported msg type %d\n", msg.type);
      request_reply(req, -ENOSYS);
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

  endpoint_init(&fs->endpoint);

  for (i = 0; i < ENDPOINT_MBOX_CAPACITY; i++) {
    struct Page *kstack;

    if ((kstack = page_alloc_one(0, 0)) == NULL)
      k_panic("out of memory");

    kstack->ref_count++;

    k_task_create(&fs->tasks[i], NULL, fs_service_task, fs, page2kva(kstack), PAGE_SIZE, 0);
    k_task_resume(&fs->tasks[i]);
  }

  return fs;
}
