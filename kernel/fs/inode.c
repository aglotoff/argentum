#include <kernel/core/assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <kernel/console.h>
#include <kernel/tty.h>
#include <kernel/time.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/dev.h>
#include <kernel/core/tick.h>

#include "ext2.h"

static struct {
  struct Inode    buf[INODE_CACHE_SIZE];
  struct KSpinLock lock;
  struct KListLink head;
} inode_cache;

void
fs_inode_cache_init(void)
{
  struct Inode *ip;
  
  k_spinlock_init(&inode_cache.lock, "inode_cache");
  k_list_init(&inode_cache.head);

  for (ip = inode_cache.buf; ip < &inode_cache.buf[INODE_CACHE_SIZE]; ip++) {
    k_mutex_init(&ip->mutex, "inode");
    k_list_add_back(&inode_cache.head, &ip->cache_link);
  }
}

struct Inode *
fs_inode_get(ino_t ino, dev_t dev)
{
  struct KListLink *l;
  struct Inode *ip, *empty;

  k_spinlock_acquire(&inode_cache.lock);

  empty = NULL;
  KLIST_FOREACH(&inode_cache.head, l) {
    ip = KLIST_CONTAINER(l, struct Inode, cache_link);
    if ((ip->ino == ino) && (ip->dev == dev) && (ip->ref_count > 0)) {
      ip->ref_count++;

      k_spinlock_release(&inode_cache.lock);

      return ip;
    }

    if (ip->ref_count == 0)
      empty = ip;
  }

  if (empty != NULL) {
    empty->ref_count = 1;
    empty->ino       = ino;
    empty->dev       = dev;
    empty->fs        = NULL;
    empty->flags     = 0;

    k_spinlock_release(&inode_cache.lock);

    return empty;
  }

  k_spinlock_release(&inode_cache.lock);

  k_panic("out of inodes\n");

  return NULL;
}

/**
 * Increment the reference counter of the given inode.
 * 
 * @param inode Pointer to the inode
 * 
 * @return Pointer to the inode.
 */
struct Inode *
fs_inode_duplicate(struct Inode *inode)
{
  k_spinlock_acquire(&inode_cache.lock);
  inode->ref_count++;
  k_spinlock_release(&inode_cache.lock);

  return inode;
}

/**
 * Release pointer to an inode.
 * 
 * @param inode Pointer to the inode to be released
 */
void
fs_inode_put(struct Inode *inode)
{   
  int r;

  if ((r = k_mutex_lock(&inode->mutex)) < 0)
    k_panic("TODO %d", r);

  if (inode->flags & FS_INODE_DIRTY)
    k_panic("inode dirty");

  // If the link count reaches zero, delete inode from the filesystem before
  // returning it to the cache
  if ((inode->flags & FS_INODE_VALID) && (inode->nlink == 0)) {
    int ref_count;

    k_spinlock_acquire(&inode_cache.lock);
    ref_count = inode->ref_count;
    k_spinlock_release(&inode_cache.lock);

    // If this is the last reference to this inode
    if (ref_count == 1) {
      struct FSMessage msg;

      msg.type = FS_MSG_INODE_DELETE;
      msg.u.inode_delete.inode = inode;

      fs_send_recv(inode->fs, &msg);

      //inode->fs->ops->inode_delete(thread_current(), inode);

      inode->flags &= ~FS_INODE_VALID;
    }
  }

  k_mutex_unlock(&inode->mutex);

  // Return the inode to the cache
  k_spinlock_acquire(&inode_cache.lock);
  if (--inode->ref_count == 0) {
    // cprintf("[inode drop %d]\n", inode->ino);

    k_list_remove(&inode->cache_link);
    k_list_add_front(&inode_cache.head, &inode->cache_link);
  }
  k_spinlock_release(&inode_cache.lock);
}

static int
fs_inode_holding(struct Inode *ip)
{
  return k_mutex_holding(&ip->mutex);
}

/**
 * Lock the given inode. Read the inode meta info, if necessary.
 * 
 * @param ip Pointer to the inode.
 */
void
fs_inode_lock(struct Inode *ip)
{
  struct FSMessage msg;

  if (k_mutex_lock(&ip->mutex) < 0)
    k_panic("TODO");

  if (ip->flags & FS_INODE_VALID)
    return;

  if (ip->flags & FS_INODE_DIRTY)
    k_panic("inode dirty");

  msg.type = FS_MSG_INODE_READ;
  msg.u.inode_read.inode = ip;

  fs_send_recv(ip->fs, &msg);

  //ip->fs->ops->inode_read(thread_current(), ip);

  ip->flags |= FS_INODE_VALID;
}

void
fs_inode_unlock(struct Inode *ip)
{  
  if (!(ip->flags & FS_INODE_VALID))
    k_panic("inode not valid");

  if (ip->flags & FS_INODE_DIRTY) {
    struct FSMessage msg;

    msg.type = FS_MSG_INODE_WRITE;
    msg.u.inode_read.inode = ip;

    fs_send_recv(ip->fs, &msg);

    //ip->fs->ops->inode_write(thread_current(), ip);
  
    ip->flags &= ~FS_INODE_DIRTY;
  }

  k_mutex_unlock(&ip->mutex);
}

int
fs_inode_open_locked(struct Inode *inode, int oflag, mode_t mode)
{
  int ret;

  if (!fs_inode_holding(inode))
    k_panic("not locked");

  // TODO: check perm

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode)) {
    struct CharDev *d = dev_lookup_char(inode->rdev);

    if (d == NULL)
      return -ENODEV;

    fs_inode_unlock(inode);
    ret = d->open(inode->rdev, oflag, mode);
    fs_inode_lock(inode);
    return ret;
  }

  return 0;
}

ssize_t
fs_inode_read_locked(struct Inode *ip, uintptr_t va, size_t nbyte, off_t *off)
{
  ssize_t ret;
  struct FSMessage msg;

    
  
  if (!fs_inode_holding(ip))
    k_panic("not locked");

  if (!fs_permission(ip, FS_PERM_READ, 0))
    return -EPERM;

  // Read from the corresponding device
  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
    struct CharDev *d = dev_lookup_char(ip->rdev);

    if (d == NULL)
      return -ENODEV;

    fs_inode_unlock(ip);
    ret = d->read(ip->rdev, va, nbyte);
    fs_inode_lock(ip);
    return ret;
  }

  if ((off_t) (*off + nbyte) < *off)
    return -EINVAL;

  if ((off_t) (*off + nbyte) > ip->size)
    nbyte = ip->size - *off;
  if (nbyte == 0)
    return 0;
    
  msg.type = FS_MSG_READ;
  msg.u.read.inode = ip;
  msg.u.read.va    = va;
  msg.u.read.nbyte = nbyte;
  msg.u.read.off   = *off;

  fs_send_recv(ip->fs, &msg);

  ret = msg.u.read.r;

  //ret = ip->fs->ops->read(thread_current(), ip, va, nbyte, *off);

  if (ret < 0)
    return ret;

  ip->atime  = time_get_seconds();
  ip->flags |= FS_INODE_DIRTY;

  *off += ret;

  return ret;
}

ssize_t
fs_inode_write_locked(struct Inode *ip, uintptr_t va, size_t nbyte, off_t *off)
{
  ssize_t total;
  struct FSMessage msg;

  if (!fs_inode_holding(ip))
    k_panic("not locked");

  if (!fs_permission(ip, FS_PERM_WRITE, 0))
    return -EPERM;

  // Write to the corresponding device
  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
    struct CharDev *d = dev_lookup_char(ip->rdev);

    if (d == NULL)
      return -ENODEV;

    fs_inode_unlock(ip);
    total = d->write(ip->rdev, va, nbyte);
    fs_inode_lock(ip);
    return total;
  }

  if ((off_t) (*off + nbyte) < *off)
    return -EINVAL;

  if (nbyte == 0)
    return 0;

  msg.type = FS_MSG_WRITE;
  msg.u.write.inode = ip;
  msg.u.write.va    = va;
  msg.u.write.nbyte = nbyte;
  msg.u.write.off   = *off;

  fs_send_recv(ip->fs, &msg);

  total = msg.u.write.r;

  //total = ip->fs->ops->write(thread_current(), ip, va, nbyte, *off);

  if (total > 0) {
    *off += total;

    if (*off > ip->size)
      ip->size = *off;

    ip->mtime = time_get_seconds();
    ip->flags |= FS_INODE_DIRTY;
  }

  return total;
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

ssize_t
fs_inode_read_dir_locked(struct Inode *ip, uintptr_t va, size_t nbyte, off_t *off)
{
  ssize_t total = 0;

  struct {
    struct dirent de;
    char buf[NAME_MAX + 1];
  } de;

  if (!fs_inode_holding(ip))
    k_panic("not locked");

  if (!S_ISDIR(ip->mode))
    return -ENOTDIR;

  if (!fs_permission(ip, FS_PERM_READ, 0))
    return -EPERM;

  while (nbyte > 0) {
    ssize_t nread;
    int r;
    struct FSMessage msg;

    msg.type = FS_MSG_READDIR;
    msg.u.readdir.inode = ip;
    msg.u.readdir.buf   = &de;
    msg.u.readdir.func  = fs_filldir;
    msg.u.readdir.off   = *off;

    fs_send_recv(ip->fs, &msg);

    nread = msg.u.readdir.r;

    // nread = ip->fs->ops->readdir(thread_current(), ip, &de, fs_filldir, *off)

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

    *off += nread;

    if ((r = vm_space_copy_out(thread_current(), &de, va, de.de.d_reclen)) < 0)
      return r;

    va    += de.de.d_reclen;
    total += de.de.d_reclen;
    nbyte -= de.de.d_reclen;
  }

  return total;
}

int
fs_inode_stat_locked(struct Inode *ip, struct stat *buf)
{
  if (!fs_inode_holding(ip))
    k_panic("not locked");

  // TODO: check permissions

  buf->st_dev          = ip->dev;
  buf->st_ino          = ip->ino;
  buf->st_mode         = ip->mode;
  buf->st_nlink        = ip->nlink;
  buf->st_uid          = ip->uid;
  buf->st_gid          = ip->gid;
  buf->st_rdev         = ip->rdev;
  buf->st_size         = ip->size;
  buf->st_atim.tv_sec  = ip->atime;
  buf->st_atim.tv_nsec = 0;
  buf->st_mtim.tv_sec  = ip->mtime;
  buf->st_mtim.tv_nsec = 0;
  buf->st_ctim.tv_sec  = ip->ctime;
  buf->st_ctim.tv_nsec = 0;
  buf->st_blocks       = ip->size / S_BLKSIZE;
  buf->st_blksize      = S_BLKSIZE;

  return 0;
}

int
fs_inode_truncate_locked(struct Inode *inode, off_t length)
{
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("not locked");

  if (length < 0)
    return -EINVAL;
  if (length > inode->size)
    return -EFBIG;

  if (!fs_permission(inode, FS_PERM_WRITE, 0))
    return -EPERM;

  msg.type = FS_MSG_TRUNC;
  msg.u.trunc.inode  = inode;
  msg.u.trunc.length = length;

  fs_send_recv(inode->fs, &msg);

  //inode->fs->ops->trunc(thread_current(), inode, length);
  
  inode->size = length;
  inode->ctime = inode->mtime = time_get_seconds();
  inode->flags |= FS_INODE_DIRTY;

  return 0;
}

int
fs_inode_create(struct Inode *dir, char *name, mode_t mode, dev_t dev,
                struct Inode **istore)
{
  struct Inode *inode;
  struct FSMessage msg;

  if (!fs_inode_holding(dir))
    k_panic("directory not locked");
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;

  msg.type = FS_MSG_LOOKUP;
  msg.u.lookup.dir  = dir;
  msg.u.lookup.name = name;

  fs_send_recv(dir->fs, &msg);

  inode = msg.u.lookup.r;

  // inode = dir->fs->ops->lookup(thread_current(), dir, name)

  if (inode != NULL) {
    fs_inode_put(inode);
    return -EEXIST;
  }

  // TODO: looks like files are created even if they do exist!!!
  // check ref_count for inode after mkdir with existing path name!

  switch (mode & S_IFMT) {
  case S_IFDIR:
    msg.type = FS_MSG_MKDIR;
    msg.u.mkdir.dir    = dir;
    msg.u.mkdir.name   = name;
    msg.u.mkdir.mode   = mode;
    msg.u.mkdir.istore = istore;

    fs_send_recv(dir->fs, &msg);

    return msg.u.mkdir.r;

    //return dir->fs->ops->mkdir(thread_current(), dir, name, mode, istore);
  case S_IFREG:
    msg.type = FS_MSG_CREATE;
    msg.u.create.dir    = dir;
    msg.u.create.name   = name;
    msg.u.create.mode   = mode;
    msg.u.create.istore = istore;

    fs_send_recv(dir->fs, &msg);

    return msg.u.create.r;

    //return dir->fs->ops->create(thread_current(), dir, name, mode, istore);
  default:
    msg.type = FS_MSG_MKNOD;
    msg.u.mknod.dir    = dir;
    msg.u.mknod.name   = name;
    msg.u.mknod.mode   = mode;
    msg.u.mknod.dev    = dev;
    msg.u.mknod.istore = istore;

    fs_send_recv(dir->fs, &msg);

    return msg.u.mknod.r;

    //return dir->fs->ops->mknod(thread_current(), dir, name, mode, dev, istore);
  }
}

int
fs_inode_link(struct Inode *inode, struct Inode *dir, char *name)
{
  // FIXME: works only in the same filesystem!
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("inode not locked");
  if (!fs_inode_holding(dir))
    k_panic("directory not locked");
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;
  
  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;
  if (inode->nlink >= LINK_MAX)
    return -EMLINK;

  if (dir->dev != inode->dev)
    return -EXDEV;

  msg.type = FS_MSG_LINK;
  msg.u.link.dir   = dir;
  msg.u.link.name  = name;
  msg.u.link.inode = inode;

  fs_send_recv(dir->fs, &msg);

  return msg.u.link.r;

  //return dir->fs->ops->link(thread_current(), dir, name, inode);
}

int
fs_inode_lookup_locked(struct Inode *dir_inode, const char *name, int flags,
                struct Inode **inode_store)
{
  struct Inode *inode;
  struct FSMessage msg;

  if (!S_ISDIR(dir_inode->mode))
    return -ENOTDIR;

  if (!fs_permission(dir_inode, FS_PERM_READ, flags & FS_LOOKUP_REAL))
    return -EPERM;

  msg.type = FS_MSG_LOOKUP;
  msg.u.lookup.dir  = dir_inode;
  msg.u.lookup.name = name;

  fs_send_recv(dir_inode->fs, &msg);

  inode = msg.u.lookup.r;

  // (ref_count): +1
  //inode = dir_inode->fs->ops->lookup(thread_current(), dir_inode, name);

  if (inode_store != NULL)  // (ref_count): pass to the caller
    *inode_store = inode;
  else if (inode != NULL)   // (ref_count): -1
    fs_inode_put(inode);

  return 0;
}

int
fs_inode_unlink(struct Inode *dir, struct Inode *inode, const char *name)
{
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("inode not locked");
  if (!fs_inode_holding(dir))
    k_panic("directory not locked");
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;

  msg.type = FS_MSG_UNLINK;
  msg.u.unlink.dir   = dir;
  msg.u.unlink.inode = inode;
  msg.u.unlink.name  = name;

  fs_send_recv(dir->fs, &msg);

  return msg.u.unlink.r;

  //return dir->fs->ops->unlink(thread_current(), dir, inode, name);
}

int
fs_inode_rmdir(struct Inode *dir, struct Inode *inode, const char *name)
{
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("inode not locked");
  if (!fs_inode_holding(dir))
    k_panic("directory not locked");
 
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (!S_ISDIR(inode->mode))
    return -EPERM;

  msg.type = FS_MSG_RMDIR;
  msg.u.rmdir.dir   = dir;
  msg.u.rmdir.inode = inode;
  msg.u.rmdir.name  = name;

  fs_send_recv(dir->fs, &msg);

  return msg.u.rmdir.r;

  //return dir->fs->ops->rmdir(thread_current(), dir, inode, name);
}

int
fs_create(const char *path, mode_t mode, dev_t dev, struct PathNode **istore)
{
  struct PathNode *dir;
  struct Inode *inode, *dir_inode;
  char name[NAME_MAX + 1];
  int r;

  // (dir->ref_count): +1
  if ((r = fs_path_lookup(path, name, 0, NULL, &dir)) < 0)
    return r;

  mode &= ~process_current()->cmask;

  dir_inode = fs_path_inode(dir);

  fs_inode_lock(dir_inode);

  // (inode->ref_count): +1
  if ((r = fs_inode_create(dir_inode, name, mode, dev, &inode)) == 0) {
    if (istore != NULL) {
      struct PathNode *pp;
      
      if ((pp = fs_path_node_create(name, inode, dir)) == NULL) {
        // fs_inode_unlock(inode);
        fs_inode_put(inode);  // (inode->ref_count): -1
        r = -ENOMEM;
      } else {
        *istore = pp;
      }
    } else {
      // fs_inode_unlock(inode);
      fs_inode_put(inode);  // (inode->ref_count): -1
    }
  }

  fs_inode_unlock(dir_inode);
  fs_inode_put(dir_inode);

  fs_path_put(dir); // (dir->ref_count): -1

  return r;
}

static void
fs_inode_lock_two(struct Inode *inode1, struct Inode *inode2)
{
  if (inode1 < inode2) {
    fs_inode_lock(inode1);
    fs_inode_lock(inode2);
  } else {
    fs_inode_lock(inode2);
    fs_inode_lock(inode1);
  }
}

static void
fs_inode_unlock_two(struct Inode *inode1, struct Inode *inode2)
{
  if (inode1 < inode2) {
    fs_inode_unlock(inode2);
    fs_inode_unlock(inode1);
  } else {
    fs_inode_unlock(inode1);
    fs_inode_unlock(inode2);
  }
}

int
fs_link(char *path1, char *path2)
{
  struct PathNode *dirp, *pp;
  struct Inode *inode, *parent_inode;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_lookup(path1, 0, &pp)) < 0)
    return r;
  if (pp == NULL)
    return -ENOENT;

  if ((r = fs_path_lookup(path2, name, 0, NULL, &dirp)) < 0)
    goto out1;

  // TODO: check for the same node?
  // TODO: lock the namespace manager?

  parent_inode = fs_path_inode(dirp);
  inode = fs_path_inode(pp);

  // Always lock inodes in a specific order to avoid deadlocks
  fs_inode_lock_two(parent_inode, inode);

  r = fs_inode_link(inode, parent_inode, name);

  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(inode);
  fs_inode_put(parent_inode);

  fs_path_put(dirp);
out1:
  fs_path_put(pp);
  return r;
}

int
fs_rename(char *old, char *new)
{
  struct PathNode *old_dir, *new_dir, *old_node, *new_node;
  struct Inode *inode, *parent_inode;
  char old_name[NAME_MAX + 1];
  char new_name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(old, old_name, 0, &old_node, &old_dir)) < 0)
    return r;
  if (old_node == NULL)
    return -ENOENT;

  if ((r = fs_path_lookup(new, new_name, 0, &new_node, &new_dir)) < 0)
    goto out1;
  if (new_dir == NULL) {
    r = -ENOENT;
    goto out1;
  }

  // TODO: check for the same node?
  // TODO: lock the namespace manager?
  // TODO: should be a transaction?

  if (new_node != NULL) {
    parent_inode = fs_path_inode(new_dir);
    inode = fs_path_inode(new_node);

    // TODO: lock the namespace manager?

    fs_inode_lock_two(parent_inode, inode);
    
    // FIXME: check if is dir

    if ((r = fs_inode_unlink(parent_inode, inode, new_name)) == 0) {
      fs_path_remove(new_node);
    }

    fs_inode_unlock_two(parent_inode, inode);

    fs_inode_put(inode);
    fs_inode_put(parent_inode);

    fs_path_put(new_node);

    if (r != 0)
      goto out2;
  }

  inode = fs_path_inode(old_node);

  parent_inode = fs_path_inode(new_dir);

  fs_inode_lock_two(parent_inode, inode);
  r = fs_inode_link(inode, parent_inode, new_name);
  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(parent_inode);

  if (r < 0)
    goto out2;

  parent_inode = fs_path_inode(old_dir);
    
  fs_inode_lock_two(parent_inode, inode);
  //cprintf("remove %s\n", );
  if ((r = fs_inode_unlink(parent_inode, inode, old_name)) == 0) {
    fs_path_remove(old_node);
  }
  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(parent_inode);

out2:
  fs_inode_put(inode);

  fs_path_put(new_dir);
out1:
  if (old_dir != NULL)
    fs_path_put(old_dir);
  if (old_node != NULL)
    fs_path_put(old_node);
  return r;
}

int
fs_unlink(const char *path)
{
  struct PathNode *dir, *pp;
  struct Inode *inode, *parent_inode;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 0, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    fs_path_put(dir);
    return -ENOENT;
  }

  parent_inode = fs_path_inode(dir);
  inode = fs_path_inode(pp);

  // TODO: lock the namespace manager?

  fs_inode_lock_two(parent_inode, inode);
  
  if ((r = fs_inode_unlink(parent_inode, inode, name)) == 0) {
    fs_path_remove(pp);
  }

  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(inode);
  fs_inode_put(parent_inode);

  fs_path_put(dir);
  fs_path_put(pp);

  return r;
}

int
fs_rmdir(const char *path)
{
  struct PathNode *dir, *pp;
  struct Inode *inode, *parent_inode;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 0, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    fs_path_put(dir);
    return -ENOENT;
  }

  // TODO: lock the namespace manager?

  parent_inode = fs_path_inode(dir);
  inode = fs_path_inode(pp);

  fs_inode_lock_two(parent_inode, inode);

  if ((r = fs_inode_rmdir(parent_inode, inode, name)) == 0) {
    fs_path_remove(pp);
  }

  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(inode);
  fs_inode_put(parent_inode);

  fs_path_put(dir);
  fs_path_put(pp);

  return r;
}

int
fs_set_pwd(struct PathNode *node)
{
  struct Process *current = process_current();
  struct Inode *inode = fs_path_inode(node);

  fs_inode_lock(inode);

  if (!S_ISDIR(inode->mode)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    return -ENOTDIR;
  }

  if (!fs_permission(inode, FS_PERM_EXEC, 0)) {
    fs_inode_unlock(inode);
    fs_inode_put(inode);
    return -EPERM;
  }

  fs_inode_unlock(inode);
  fs_inode_put(inode);

  fs_path_put(current->cwd);

  current->cwd = node;

  return 0;
}

int
fs_chdir(const char *path)
{
  struct PathNode *pp;
  int r;

  if ((r = fs_lookup(path, 0, &pp)) < 0)
    return r;

  if (pp == NULL)
    return -ENOENT;

  if ((r = fs_set_pwd(pp)) != 0)
    fs_path_put(pp);

  return r;
}

#define CHMOD_MASK  (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)

int
fs_inode_chmod_locked(struct Inode *inode, mode_t mode)
{
  struct Process *current = process_current();

  if ((current->euid != 0) && (inode->uid != current->euid)) {
    return -EPERM;
  }

  // TODO: additional permission checks

  inode->mode  = (inode->mode & ~CHMOD_MASK) | (mode & CHMOD_MASK);
  inode->ctime = time_get_seconds();
  inode->flags |= FS_INODE_DIRTY;

  return 0;
}

int
fs_permission(struct Inode *inode, mode_t mode, int real)
{
  struct Process *my_process = process_current();

  uid_t uid = real ? my_process->ruid : my_process->euid;
  gid_t gid = real ? my_process->rgid : my_process->egid;

  if (uid == 0)
    return (mode & FS_PERM_EXEC)
      ? ((inode->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)
      : 1;

  // TODO: support S_ISVTX

  if (uid == inode->uid)
    mode <<= 6;
  else if (gid == inode->gid)
    mode <<= 3;

  return (inode->mode & mode) == mode;
}

int
fs_inode_access(struct Inode *inode, int amode)
{
  int r = 0;

  fs_inode_lock(inode);

  if ((amode & R_OK) && !fs_permission(inode, FS_PERM_READ, 1))
    r = -EPERM;
  if ((amode & W_OK) && !fs_permission(inode, FS_PERM_WRITE, 1))
    r = -EPERM;
  if ((amode & X_OK) && !fs_permission(inode, FS_PERM_EXEC, 1))
    r = -EPERM;

  fs_inode_unlock(inode);

  return r;
}

int
fs_inode_utime(struct Inode *inode, struct utimbuf *times)
{
  struct Process *my_process = process_current();
  int r = 0;

  fs_inode_lock(inode);

  if (times == NULL) {
    if ((my_process->euid != 0) &&
        (my_process->euid != inode->uid) &&
        !fs_permission(inode, FS_PERM_WRITE, 1)) {
      r = -EPERM;
      goto out;
    }

    inode->atime = time_get_seconds();
    inode->mtime = time_get_seconds();

    inode->flags |= FS_INODE_DIRTY;
  } else {
    if ((my_process->euid != 0) ||
       ((my_process->euid != inode->uid) ||
         !fs_permission(inode, FS_PERM_WRITE, 1))) {
      r = -EPERM;
      goto out;
    }

    inode->atime = times->actime;
    inode->mtime = times->modtime;

    inode->flags |= FS_INODE_DIRTY;
  }

out:
  fs_inode_unlock(inode);

  return r;
}

int
fs_inode_ioctl_locked(struct Inode *inode, int request, int arg)
{
  int ret;

  if (!fs_inode_holding(inode))
    k_panic("not locked");

  // TODO: check perm

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode)) {
    struct CharDev *d = dev_lookup_char(inode->rdev);

    if (d == NULL)
      return -ENODEV;

    fs_inode_unlock(inode);
    ret = d->ioctl(inode->rdev, request, arg);
    fs_inode_lock(inode);
    return ret;
  }

  return -ENOTTY;
}

int
fs_inode_select_locked(struct Inode *inode, struct timeval *timeout)
{
  int ret;

  if (!fs_inode_holding(inode))
    k_panic("not locked");

  // TODO: check perm

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode)) {
    struct CharDev *d = dev_lookup_char(inode->rdev);

    if (d == NULL)
      return -ENODEV;

    fs_inode_unlock(inode);
    ret = d->select(inode->rdev, timeout);
    fs_inode_lock(inode);
    return ret;
  }

  return 1;
}

int
fs_inode_sync_locked(struct Inode *inode)
{
  if (!fs_inode_holding(inode))
    k_panic("not locked");

  // TODO: implement

  return 0;
}

int
fs_inode_chown_locked(struct Inode *inode, uid_t uid, gid_t gid)
{
  struct Process *current = process_current();

  if ((uid != (uid_t) -1) &&
      (current->euid != 0) &&
      (current->euid != inode->uid))
    return -EPERM;

  if ((gid != (gid_t) -1) &&
      (current->euid != 0) &&
      (current->euid != inode->uid) &&
      ((uid != (uid_t) -1) || (current->egid != inode->gid)))
    return -EPERM;

  if (uid != (uid_t) -1)
    inode->uid = uid;

  if (gid != (gid_t) -1)
    inode->gid = gid;

  return 0;
}

ssize_t
fs_inode_readlink(struct Inode *inode, char *buf, size_t bufsize)
{ 
  ssize_t r;
  
  fs_inode_lock(inode);

  if (!fs_permission(inode, FS_PERM_READ, 0)) {
    fs_inode_unlock(inode);
    return -EPERM;
  }

  if (!S_ISLNK(inode->mode)) {
    fs_inode_unlock(inode);
    return -EINVAL;
  }

  // FIXME: readlink
  r = inode->fs->ops->readlink(thread_current(), inode, buf, bufsize);

  fs_inode_unlock(inode);

  return r;
}
