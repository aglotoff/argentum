#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kernel/fs/fs.h>
#include <kernel/ipc/channel.h>
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
  ino_t ino;
  struct FS *fs;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &fs);

  msg.type = FS_MSG_ACCESS;
  msg.u.access.ino   = ino;
  msg.u.access.amode = amode;

  fs_send_recv(fs, &msg);

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
  ino_t ino;
  struct FS *fs;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &fs);

  msg.type = FS_MSG_CHMOD;
  msg.u.chmod.ino  = ino;
  msg.u.chmod.mode = mode;

  fs_send_recv(fs, &msg);

  fs_path_node_unref(node);

  return msg.u.chmod.r;
}

int
fs_chown(const char *path, uid_t uid, gid_t gid)
{
  struct FSMessage msg;
  struct PathNode *node;
  ino_t ino;
  struct FS *fs;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &fs);

  msg.type = FS_MSG_CHOWN;
  msg.u.chown.ino = ino;
  msg.u.chown.uid = uid;
  msg.u.chown.gid = gid;

  fs_send_recv(fs, &msg);

  fs_path_node_unref(node);

  return msg.u.chown.r;
}

int
fs_create(const char *path, mode_t mode, dev_t dev, struct PathNode **istore)
{
  struct FSMessage msg;
  struct PathNode *dir;
  ino_t ino, dir_ino;
  struct FS *fs;
  char name[NAME_MAX + 1];
  int r;

  // REF(dir)
  if ((r = fs_path_node_resolve(path, name, FS_LOOKUP_FOLLOW_LINKS, NULL, &dir)) < 0)
    return r;

  mode &= ~process_current()->cmask;

  dir_ino = fs_path_ino(dir, &fs);

  msg.type = FS_MSG_CREATE;
  msg.u.create.dir_ino = dir_ino;
  msg.u.create.name    = name;
  msg.u.create.mode    = mode;
  msg.u.create.dev     = dev;
  msg.u.create.istore  = &ino;

  fs_send_recv(fs, &msg);

  // (inode->ref_count): +1
  if ((r = msg.u.create.r) == 0) {
    if (istore != NULL) {
      struct PathNode *pp;
      
      // REF(pp)
      if ((pp = fs_path_node_create(name, ino, fs, dir)) == NULL) {
        // fs_inode_unlock(inode);
        r = -ENOMEM;
      } else {
        *istore = pp;
      }
    } else {
      // fs_inode_unlock(inode);
      //fs_inode_put(inode);  // (inode->ref_count): -1
    }
  }

  // UNREF(dir)
  fs_path_node_unref(dir);
  dir = NULL;

  return r;
}

int
fs_link(char *path1, char *path2)
{
  struct FSMessage msg;
  struct PathNode *dirp, *pp;
  ino_t ino, parent_ino;
  struct FS *fs, *parent_fs;
  char name[NAME_MAX + 1];
  int r;

  // REF(pp)
  if ((r = fs_path_resolve(path1, 0, &pp)) < 0)
    return r;
  if (pp == NULL)
    return -ENOENT;

  // REF(dirp)
  if ((r = fs_path_node_resolve(path2, name, FS_LOOKUP_FOLLOW_LINKS, NULL, &dirp)) < 0)
    goto out1;

  // TODO: check for the same node?
  // TODO: lock the namespace manager?

  parent_ino = fs_path_ino(dirp, &parent_fs);
  ino = fs_path_ino(pp, &fs);

  k_assert(fs == parent_fs);

  msg.type = FS_MSG_LINK;
  msg.u.link.dir_ino = parent_ino;
  msg.u.link.name    = name;
  msg.u.link.ino     = ino;

  fs_send_recv(parent_fs, &msg);

  r = msg.u.link.r;

  // UNREF(dirp)
  fs_path_node_unref(dirp);
out1:
  // UNREF(pp)
  fs_path_node_unref(pp);
  return r;
}

ssize_t
fs_readlink(const char *path, uintptr_t va, size_t bufsize)
{
  struct FSMessage msg;  
  struct PathNode *node;
  ino_t ino;
  struct FS *fs;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &fs);

  msg.type = FS_MSG_READLINK;
  msg.u.readlink.ino   = ino;
  msg.u.readlink.va    = va;
  msg.u.readlink.nbyte = bufsize;

  fs_send_recv(fs, &msg);

  fs_path_node_unref(node);

  return msg.u.readlink.r;
}

int
fs_rename(char *old, char *new)
{
  struct PathNode *old_dir, *new_dir, *old_node, *new_node;
  ino_t ino, parent_ino;
  struct FS *fs, *parent_fs;
  struct FSMessage msg;
  char old_name[NAME_MAX + 1];
  char new_name[NAME_MAX + 1];
  int r;

  // REF(old_node), REF(old_dir)
  if ((r = fs_path_node_resolve(old, old_name, FS_LOOKUP_FOLLOW_LINKS, &old_node, &old_dir)) < 0)
    return r;
  if (old_node == NULL)
    return -ENOENT;

  // REF(new_node), REF(new_dir)
  if ((r = fs_path_node_resolve(new, new_name, FS_LOOKUP_FOLLOW_LINKS, &new_node, &new_dir)) < 0)
    goto out1;
  if (new_dir == NULL) {
    r = -ENOENT;
    goto out1;
  }

  // TODO: check for the same node?
  // TODO: lock the namespace manager?
  // TODO: should be a transaction?

  if (new_node != NULL) {
    parent_ino = fs_path_ino(new_dir, &parent_fs);
    ino = fs_path_ino(new_node, &fs);

    // TODO: lock the namespace manager?
    // FIXME: check if is dir

    k_assert(parent_fs == fs);

    msg.type = FS_MSG_UNLINK;
    msg.u.unlink.dir_ino = parent_ino;
    msg.u.unlink.ino     = ino;
    msg.u.unlink.name    = new_name;
    
    fs_send_recv(parent_fs, &msg);

    if ((r = msg.u.unlink.r) == 0) {
      fs_path_node_remove(new_node);
    }

    // UNREF(new_node)
    fs_path_node_unref(new_node);
    new_node = NULL;

    if (r != 0)
      goto out2;
  }

  ino        = fs_path_ino(old_node, &fs);
  parent_ino = fs_path_ino(new_dir, &parent_fs);

  k_assert(parent_fs == fs);

  msg.type = FS_MSG_LINK;
  msg.u.link.dir_ino = parent_ino;
  msg.u.link.name    = new_name;
  msg.u.link.ino     = ino;

  fs_send_recv(parent_fs, &msg);

  r = msg.u.link.r;

  if (r < 0)
    goto out2;

  parent_ino = fs_path_ino(old_dir, &parent_fs);

  k_assert(parent_fs == fs);
    
  msg.type = FS_MSG_UNLINK;
  msg.u.unlink.dir_ino = parent_ino;
  msg.u.unlink.ino     = ino;
  msg.u.unlink.name    = old_name;

  fs_send_recv(parent_fs, &msg);

  if ((r = msg.u.unlink.r) == 0) {
    fs_path_node_remove(old_node);
  }

out2:
  // UNREF(new_dir)
  fs_path_node_unref(new_dir);

out1:
  // UNREF(old_dir)
  if (old_dir != NULL)
    fs_path_node_unref(old_dir);
  // UNREF(old_node)
  if (old_node != NULL)
    fs_path_node_unref(old_node);
  return r;
}

int
fs_rmdir(const char *path)
{
  struct FSMessage msg;
  struct PathNode *dir, *pp;
  ino_t ino, parent_ino;
  struct FS *fs, *parent_fs;
  char name[NAME_MAX + 1];
  int r;

  // REF(pp), REF(dir)
  if ((r = fs_path_node_resolve(path, name, 0, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    // UNREF(dir)
    fs_path_node_unref(dir);
  
    return -ENOENT;
  }

  // TODO: lock the namespace manager?

  parent_ino = fs_path_ino(dir, &parent_fs);
  ino = fs_path_ino(pp, &fs);

  k_assert(fs == parent_fs);

  msg.type = FS_MSG_RMDIR;
  msg.u.rmdir.dir_ino = parent_ino;
  msg.u.rmdir.ino     = ino;
  msg.u.rmdir.name    = pp->name;

  fs_send_recv(parent_fs, &msg);

  if ((r = msg.u.rmdir.r) == 0) {
    fs_path_node_remove(pp);
  }

  // UNREF(dir)
  fs_path_node_unref(dir);

  // UNREF(pp)
  fs_path_node_unref(pp);

  return r;
}

int
fs_unlink(const char *path)
{
  struct FSMessage msg;
  struct PathNode *dir, *pp;
  ino_t ino, parent_ino;
  struct FS *fs, *parent_fs;
  char name[NAME_MAX + 1];
  int r;

  // REF(pp), REF(dir)
  if ((r = fs_path_node_resolve(path, name, FS_LOOKUP_FOLLOW_LINKS, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    // UNREF(dir)
    fs_path_node_unref(dir);
    return -ENOENT;
  }

  parent_ino = fs_path_ino(dir, &parent_fs);
  ino        = fs_path_ino(pp, &fs);

  k_assert(parent_fs == fs);

  // TODO: lock the namespace manager?

  msg.type = FS_MSG_UNLINK;
  msg.u.unlink.dir_ino = parent_ino;
  msg.u.unlink.ino     = ino;
  msg.u.unlink.name    = pp->name;
    
  fs_send_recv(parent_fs, &msg);

  if ((r = msg.u.unlink.r) == 0) {
    fs_path_node_remove(pp);
  }

  // UNREF(dir)
  fs_path_node_unref(dir);

  // UNREF(pp)
  fs_path_node_unref(pp);

  return r;
}

int
fs_utime(const char *path, struct utimbuf *times)
{
  struct FSMessage msg;
  struct PathNode *node;
  ino_t ino;
  struct FS *fs;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &fs);

  msg.type = FS_MSG_UTIME;
  msg.u.utime.ino   = ino;
  msg.u.utime.times = times;

  fs_send_recv(fs, &msg);

  fs_path_node_unref(node);

  return msg.u.utime.r;
}

/*
 * ----- File Operations ----- 
 */

int
fs_close(struct Channel *file)
{
  k_assert(file->type == CHANNEL_TYPE_FILE);

  // TODO: add a comment, when node can be NULL?
  if (file->u.file.node != NULL) {
    struct FSMessage msg;

    msg.type = FS_MSG_CLOSE;
    msg.u.close.file = file;

    fs_send_recv(file->u.file.fs, &msg);

    fs_path_node_unref(file->u.file.node);
    file->u.file.node = NULL;
  }

  return 0;
}
 
int
fs_fchdir(struct Channel *file)
{
  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  return fs_path_set_cwd(file->u.file.node);
}

int
fs_fchmod(struct Channel *file, mode_t mode)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  msg.type = FS_MSG_FCHMOD;
  msg.u.fchmod.file = file;
  msg.u.fchmod.mode = mode;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.fchmod.r;
}

int
fs_fchown(struct Channel *file, uid_t uid, gid_t gid)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  msg.type = FS_MSG_FCHOWN;
  msg.u.fchown.file = file;
  msg.u.fchown.uid  = uid;
  msg.u.fchown.gid  = gid;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.fchown.r;
}

int
fs_fstat(struct Channel *file, struct stat *buf)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  msg.type = FS_MSG_FSTAT;
  msg.u.fstat.file = file;
  msg.u.fstat.buf  = buf;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.fstat.r;
}

int
fs_fsync(struct Channel *file)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  msg.type = FS_MSG_FSYNC;
  msg.u.fsync.file = file;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.fsync.r;
}

int
fs_ftruncate(struct Channel *file, off_t length)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  msg.type = FS_MSG_TRUNC;
  msg.u.trunc.file   = file;
  msg.u.trunc.length = length;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.trunc.r;
}

ssize_t
fs_getdents(struct Channel *file, uintptr_t va, size_t nbyte)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  if ((file->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  msg.type = FS_MSG_READDIR;
  msg.u.readdir.file = file;
  msg.u.readdir.va    = va;
  msg.u.readdir.nbyte = nbyte;
  
  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.readdir.r;
}

int
fs_ioctl(struct Channel *file, int request, int arg)
{
  struct FSMessage msg;
  
  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  if (file->u.file.rdev >= 0)
    return dev_ioctl(thread_current(), file->u.file.rdev, request, arg);

  msg.type = FS_MSG_IOCTL;
  msg.u.ioctl.file    = file;
  msg.u.ioctl.request = request;
  msg.u.ioctl.arg     = arg;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.ioctl.r;
}

#define OPEN_TIME_FLAGS (O_CREAT | O_EXCL | O_DIRECTORY | O_NOFOLLOW | O_NOCTTY | O_TRUNC)

int
fs_open(const char *path, int oflag, mode_t mode, struct Channel **file_store)
{
  struct FSMessage msg;
  struct Channel *file;
  struct PathNode *path_node;
  ino_t ino;
  struct FS *fs;
  int r, flags;

  // TODO: O_NONBLOCK

  //if (oflag & O_SEARCH) k_panic("O_SEARCH %s", path);
  //if (oflag & O_EXEC) k_panic("O_EXEC %s", path);
  //if (oflag & O_NOFOLLOW) k_panic("O_NOFOLLOW %s", path);
  if (oflag & O_SYNC) k_panic("O_SYNC %s", path);
  if (oflag & O_DIRECT) k_panic("O_DIRECT %s", path);

  // TODO: ENFILE
  if ((r = channel_alloc(&file)) != 0)
    return r;

  file->flags        = oflag & ~OPEN_TIME_FLAGS;
  file->type         = CHANNEL_TYPE_FILE;
  file->u.file.node  = NULL;
  file->u.file.inode = NULL;
  file->u.file.fs    = NULL;
  file->u.file.rdev  = -1;
  file->ref_count    = 1;

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

  ino = fs_path_ino(path_node, &fs);

  msg.type = FS_MSG_OPEN;
  msg.u.open.file  = file;
  msg.u.open.ino = ino;
  msg.u.open.oflag = oflag;

  fs_send_recv(fs, &msg);

  if ((r = msg.u.open.r) < 0)
    goto out2;

  if (file->u.file.rdev >= 0) {
    if ((r = dev_open(thread_current(), file->u.file.rdev, oflag, mode)) < 0)
      goto out2;
  }

  // REF(file->u.file.node)
  file->u.file.node = fs_path_node_ref(path_node);
  file->u.file.fs   = fs;

  // UNREF(path_node)
  fs_path_node_unref(path_node);
  path_node = NULL;

  *file_store = file;

  return 0;
out2:
  // UNREF(path_node)
  fs_path_node_unref(path_node);
out1:
  channel_unref(file);
  return r;
}

ssize_t
fs_read(struct Channel *file, uintptr_t va, size_t nbytes)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  if (file->u.file.rdev >= 0)
    return dev_read(thread_current(), file->u.file.rdev, va, nbytes);

  msg.type = FS_MSG_READ;
  msg.u.read.file  = file;
  msg.u.read.va    = va;
  msg.u.read.nbyte = nbytes;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.read.r;
}

off_t
fs_seek(struct Channel *file, off_t offset, int whence)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  msg.type = FS_MSG_SEEK;
  msg.u.seek.file   = file;
  msg.u.seek.offset = offset;
  msg.u.seek.whence = whence;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.seek.r;
}

int
fs_select(struct Channel *file, struct timeval *timeout)
{
  struct FSMessage msg;

  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  if (file->u.file.rdev >= 0)
    return dev_select(thread_current(), file->u.file.rdev, timeout);

  msg.type = FS_MSG_SELECT;
  msg.u.select.file    = file;
  msg.u.select.timeout = timeout;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.select.r;
}

ssize_t
fs_write(struct Channel *file, uintptr_t va, size_t nbytes)
{
  struct FSMessage msg;
  
  k_assert(file->ref_count > 0);
  k_assert(file->type == CHANNEL_TYPE_FILE);

  if (file->u.file.rdev >= 0)
    return dev_write(thread_current(), file->u.file.rdev, va, nbytes);

  msg.type = FS_MSG_WRITE;
  msg.u.write.file  = file;
  msg.u.write.va    = va;
  msg.u.write.nbyte = nbytes;

  fs_send_recv(file->u.file.fs, &msg);

  return msg.u.write.r;
}
