#include <kernel/assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/console.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/process.h>

#include "ext2.h"

static struct {
  struct Inode    buf[INODE_CACHE_SIZE];
  struct SpinLock lock;
  struct ListLink head;
} inode_cache;

void
fs_inode_cache_init(void)
{
  struct Inode *ip;
  
  spin_init(&inode_cache.lock, "inode_cache");
  list_init(&inode_cache.head);

  for (ip = inode_cache.buf; ip < &inode_cache.buf[INODE_CACHE_SIZE]; ip++) {
    kmutex_init(&ip->mutex, "inode");
    list_add_back(&inode_cache.head, &ip->cache_link);
  }
}

struct Inode *
fs_inode_get(ino_t ino, dev_t dev)
{
  struct ListLink *l;
  struct Inode *ip, *empty;

  spin_lock(&inode_cache.lock);

  empty = NULL;
  LIST_FOREACH(&inode_cache.head, l) {
    ip = LIST_CONTAINER(l, struct Inode, cache_link);
    if ((ip->ino == ino) && (ip->dev == dev)) {
      ip->ref_count++;
      spin_unlock(&inode_cache.lock);

      return ip;
    }

    if (ip->ref_count == 0)
      empty = ip;
  }

  if (empty != NULL) {
    empty->ref_count = 1;
    empty->ino       = ino;
    empty->dev       = dev;
    empty->flags     = 0;

    spin_unlock(&inode_cache.lock);

    return empty;
  }

  spin_unlock(&inode_cache.lock);

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
  spin_lock(&inode_cache.lock);
  inode->ref_count++;
  spin_unlock(&inode_cache.lock);

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
  kmutex_lock(&inode->mutex);

  if (inode->flags & FS_INODE_DIRTY)
    panic("inode dirty");

  // If the link count reaches zero, delete inode from the filesystem before
  // returning it to the cache
  if ((inode->flags & FS_INODE_VALID) && (inode->nlink == 0)) {
    int ref_count;

    spin_lock(&inode_cache.lock);
    ref_count = inode->ref_count;
    spin_unlock(&inode_cache.lock);

    // If this is the last reference to this inode
    if (ref_count == 1) {
      inode->fs->ops->inode_delete(inode);
      inode->flags &= ~FS_INODE_VALID;
    }
  }

  kmutex_unlock(&inode->mutex);

  // Return the inode to the cache
  spin_lock(&inode_cache.lock);
  if (--inode->ref_count == 0) {
    list_remove(&inode->cache_link);
    list_add_front(&inode_cache.head, &inode->cache_link);
  }
  spin_unlock(&inode_cache.lock);
}

static int
fs_inode_holding(struct Inode *ip)
{
  return kmutex_holding(&ip->mutex);
}

/**
 * Lock the given inode. Read the inode meta info, if necessary.
 * 
 * @param ip Pointer to the inode.
 */
void
fs_inode_lock(struct Inode *ip)
{
  kmutex_lock(&ip->mutex);

  if (ip->flags & FS_INODE_VALID)
    return;

  if (ip->flags & FS_INODE_DIRTY)
    panic("inode dirty");

  ip->fs->ops->inode_read(ip);

  ip->flags |= FS_INODE_VALID;
}

void
fs_inode_unlock(struct Inode *ip)
{  
  if (!(ip->flags & FS_INODE_VALID))
    panic("inode not valid");

  if (ip->flags & FS_INODE_DIRTY) {
    ip->fs->ops->inode_write(ip);
    ip->flags &= ~FS_INODE_DIRTY;
  }

  kmutex_unlock(&ip->mutex);
}

/**
 * Common pattern: unlock inode and then put.
 *
 * @param ip Pointer to the inode.
 */
// void
// fs_inode_unlock_put(struct Inode *ip)
// {  
//   fs_inode_unlock(ip);
//   fs_inode_put(ip);
// }

ssize_t
fs_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t *off)
{
  ssize_t ret;
  
  if (!fs_inode_holding(ip))
    panic("not locked");

  if (!fs_permission(ip, FS_PERM_READ, 0))
    return -EPERM;

  // Read from the corresponding device
  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
    // cprintf("Read %x\n", ip->rdev);

    if ((ip->rdev & 0xFF00) == 0x0100) {
      fs_inode_unlock(ip);

      // TODO: support other devices
      ret = console_read(ip, (uintptr_t) buf, nbyte);

      fs_inode_lock(ip);
      return ret;
    } else {
       return nbyte;
    }
  }

  if ((off_t) (*off + nbyte) < *off)
    return -EINVAL;

  if ((off_t) (*off + nbyte) > ip->size)
    nbyte = ip->size - *off;
  if (nbyte == 0)
    return 0;

  if ((ret = ip->fs->ops->read(ip, buf, nbyte, *off)) < 0)
    return ret;

  ip->atime  = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  *off += ret;

  return ret;
}

ssize_t
fs_inode_write(struct Inode *ip, const void *buf, size_t nbyte, off_t *off)
{
  ssize_t total;

  if (!fs_inode_holding(ip))
    panic("not locked");

  if (!fs_permission(ip, FS_PERM_WRITE, 0))
    return -EPERM;

  // Write to the corresponding device
  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
    // cprintf("Write %x\n", ip->rdev);

    if ((ip->rdev & 0xFF00) == 0x0100) {
      fs_inode_unlock(ip);

      // TODO: support other devices
      total = console_write(ip, buf, nbyte);

      fs_inode_lock(ip);
      return total;
    } else {
      return nbyte;
    }
  }

  if ((off_t) (*off + nbyte) < *off)
    return -EINVAL;

  if (nbyte == 0)
    return 0;

  total = ip->fs->ops->write(ip, buf, nbyte, *off);

  if (total > 0) {
    *off += total;

    if (*off > ip->size)
      ip->size = *off;

    ip->mtime = rtc_get_time();
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
fs_inode_read_dir(struct Inode *ip, void *buf, size_t nbyte, off_t *off)
{
  char *dst = (char *) buf;
  ssize_t total = 0;

  struct {
    struct dirent de;
    char buf[NAME_MAX + 1];
  } de;

  if (!fs_inode_holding(ip))
    panic("not locked");

  if (!fs_permission(ip, FS_PERM_READ, 0))
    return -EPERM;

  while (nbyte > 0) {
    ssize_t nread;

    if ((nread = ip->fs->ops->readdir(ip, &de, fs_filldir, *off)) < 0)
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

    memmove(dst, &de, de.de.d_reclen);

    dst   += de.de.d_reclen;
    total += de.de.d_reclen;
    nbyte -= de.de.d_reclen;
  }

  return total;
}

int
fs_inode_stat(struct Inode *ip, struct stat *buf)
{
  if (!fs_inode_holding(ip))
    panic("not locked");

  // TODO: check permissions

  buf->st_mode  = ip->mode;
  buf->st_ino   = ip->ino;
  buf->st_dev   = ip->dev;
  buf->st_nlink = ip->nlink;
  buf->st_uid   = ip->uid;
  buf->st_gid   = ip->gid;
  buf->st_size  = ip->size;
  buf->st_rdev  = ip->rdev;
  buf->st_atime = ip->atime;
  buf->st_mtime = ip->mtime;
  buf->st_ctime = ip->ctime;

  return 0;
}

int
fs_inode_truncate(struct Inode *inode)
{
  if (!fs_inode_holding(inode))
    panic("not locked");

  if (!fs_permission(inode, FS_PERM_WRITE, 0))
    return -EPERM;

  inode->fs->ops->trunc(inode, 0);
  
  inode->size = 0;
  inode->ctime = inode->mtime = rtc_get_time();
  inode->flags |= FS_INODE_DIRTY;

  return 0;
}

int
fs_inode_create(struct Inode *dir, char *name, mode_t mode, dev_t dev,
                struct Inode **istore)
{
  if (!fs_inode_holding(dir))
    panic("directory not locked");
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;

  if (dir->fs->ops->lookup(dir, name) != NULL)
    return -EEXIST;

  switch (mode & S_IFMT) {
  case S_IFDIR:
    return dir->fs->ops->mkdir(dir, name, mode, istore);
  case S_IFREG:
    return dir->fs->ops->create(dir, name, mode, istore);
  default:
    return dir->fs->ops->mknod(dir, name, mode, dev, istore);
  }
}

int
fs_inode_link(struct Inode *inode, struct Inode *dir, char *name)
{
  if (!fs_inode_holding(inode))
    panic("inode not locked");
  if (!fs_inode_holding(dir))
    panic("directory not locked");
  
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

  return dir->fs->ops->link(dir, name, inode);
}

int
fs_inode_lookup(struct Inode *dir, const char *name, int real,
                struct Inode **istore)
{
  struct Inode *inode;

  fs_inode_lock(dir);
  
  if (!S_ISDIR(dir->mode)) {
    fs_inode_unlock(dir);
    return -ENOTDIR;
  }

  if (!fs_permission(dir, FS_PERM_READ, real)) {
    fs_inode_unlock(dir);
    return -EPERM;
  }

  inode = dir->fs->ops->lookup(dir, name);

  if (istore != NULL)
    *istore = inode;
  else if (inode == NULL)
    fs_inode_put(inode);

  fs_inode_unlock(dir);

  return 0;
}

int
fs_inode_unlink(struct Inode *dir, struct Inode *inode)
{
  if (!fs_inode_holding(inode))
    panic("inode not locked");
  if (!fs_inode_holding(dir))
    panic("directory not locked");
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;

  return dir->fs->ops->unlink(dir, inode);
}

int
fs_inode_rmdir(struct Inode *dir, struct Inode *inode)
{
  if (!fs_inode_holding(inode))
    panic("inode not locked");
  if (!fs_inode_holding(dir))
    panic("directory not locked");
 
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (!S_ISDIR(inode->mode))
    return -EPERM;

  return dir->fs->ops->rmdir(dir, inode);
}

int
fs_create(const char *path, mode_t mode, dev_t dev, struct PathNode **istore)
{
  struct PathNode *dir;
  struct Inode *inode;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 0, NULL, &dir)) < 0)
    return r;

  mode &= ~process_current()->cmask;

  fs_inode_lock(dir->inode);

  if ((r = fs_inode_create(dir->inode, name, mode, dev, &inode)) == 0) {
    if (istore != NULL) {
      struct PathNode *pp;
      
      if ((pp = fs_path_create(name, inode, dir)) == NULL) {
        fs_inode_unlock(inode);
        fs_inode_put(inode);
        r = -ENOMEM;
      } else {
        *istore = pp;
      }
    } else {
      fs_inode_unlock(inode);
      fs_inode_put(inode);
    }
  }

  fs_inode_unlock(dir->inode);

  fs_path_put(dir);

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

  // Always lock inodes in a specific order to avoid deadlocks
  fs_inode_lock_two(dirp->inode, pp->inode);

  r = fs_inode_link(pp->inode, dirp->inode, name);

  fs_inode_unlock_two(dirp->inode, pp->inode);

  fs_path_put(dirp);
out1:
  fs_path_put(pp);
  return r;
}

int
fs_unlink(const char *path)
{
  struct PathNode *dir, *pp;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 0, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    fs_path_put(dir);
    return -ENOENT;
  }

  // TODO: lock the namespace manager?

  fs_inode_lock_two(dir->inode, pp->inode);
  
  if ((r = fs_inode_unlink(dir->inode, pp->inode)) == 0) {
    fs_path_remove(pp);
  }

  fs_inode_unlock_two(dir->inode, pp->inode);

  fs_path_put(dir);
  fs_path_put(pp);

  return r;
}

int
fs_rmdir(const char *path)
{
  struct PathNode *dir, *pp;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 0, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    fs_path_put(dir);
    return -ENOENT;
  }

  // TODO: lock the namespace manager?

  fs_inode_lock_two(dir->inode, pp->inode);

  if ((r = fs_inode_rmdir(dir->inode, pp->inode)) == 0) {
    fs_path_remove(pp);
  }

  fs_inode_unlock_two(dir->inode, pp->inode);

  fs_path_put(dir);
  fs_path_put(pp);

  return r;
}

int
fs_set_pwd(struct PathNode *node)
{
  struct Process *current = process_current();

  fs_inode_lock(node->inode);

  if (!S_ISDIR(node->inode->mode)) {
    fs_inode_unlock(node->inode);
    return -ENOTDIR;
  }
  if (!fs_permission(node->inode, FS_PERM_EXEC, 0)) {
    fs_inode_unlock(node->inode);
    return -EPERM;
  }

  fs_inode_unlock(node->inode);

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
fs_inode_chmod(struct Inode *inode, mode_t mode)
{
  struct Process *current = process_current();

  fs_inode_lock(inode);

  if ((current->euid != 0) && (inode->uid != current->euid)) {
    fs_inode_unlock(inode);
    return -EPERM;
  }

  // TODO: additional permission checks

  inode->mode  = (inode->mode & ~CHMOD_MASK) | (mode & CHMOD_MASK);
  inode->ctime = rtc_get_time();
  inode->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(inode);

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
fs_inode_ioctl(struct Inode *inode, int request, int arg)
{
  int ret;

  if (!fs_inode_holding(inode))
    panic("not locked");

  // TODO: check perm

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode)) {
    if ((inode->rdev & 0xFF00) == 0x0100) {
      fs_inode_unlock(inode);

      // TODO: support other devices
      ret = console_ioctl(inode, request, arg);

      fs_inode_lock(inode);
      return ret;
    }
  }

  return -ENOTTY;
}
