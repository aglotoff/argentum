#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>

#include <argentum/cprintf.h>
#include <argentum/drivers/console.h>
#include <argentum/drivers/rtc.h>
#include <argentum/fs/buf.h>
#include <argentum/fs/fs.h>
#include <argentum/process.h>

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
    list_init(&ip->wait_queue);

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

void
fs_inode_put(struct Inode *ip)
{   
  kmutex_lock(&ip->mutex);

  if (ip->nlink == 0) {
    int r;

    spin_lock(&inode_cache.lock);
    r = ip->ref_count;
    spin_unlock(&inode_cache.lock);

    if (r == 1) {
      ext2_delete_inode(ip);
      ip->flags = 0;
    }
  }

  kmutex_unlock(&ip->mutex);

  spin_lock(&inode_cache.lock);

  if (--ip->ref_count == 0) {
    list_remove(&ip->cache_link);
    list_add_front(&inode_cache.head, &ip->cache_link);
  }

  spin_unlock(&inode_cache.lock);
}

void
fs_inode_lock(struct Inode *ip)
{
  kmutex_lock(&ip->mutex);

  if (ip->flags & FS_INODE_VALID)
    return;

  if (ip->flags & FS_INODE_DIRTY)
    panic("inode dirty");

  // panic("AA");

  ext2_read_inode(ip);

  if (!ip->mode)
    panic("no mode");

  ip->flags |= FS_INODE_VALID;
}

struct Inode *
fs_inode_dup(struct Inode *ip)
{
  spin_lock(&inode_cache.lock);
  ip->ref_count++;
  spin_unlock(&inode_cache.lock);

  return ip;
}

void
fs_inode_unlock(struct Inode *ip)
{  
  if (!kmutex_holding(&ip->mutex))
    panic("not holding buf");

  if (!(ip->flags & FS_INODE_VALID))
    panic("inode nt valid");

  if (ip->flags & FS_INODE_DIRTY) {
    ext2_write_inode(ip);
    ip->flags &= ~FS_INODE_DIRTY;
  }

  kmutex_unlock(&ip->mutex);
}

void
fs_inode_unlock_put(struct Inode *ip)
{  
  fs_inode_unlock(ip);
  fs_inode_put(ip);
}

ssize_t
fs_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t *off)
{
  ssize_t ret;
  
  if (!kmutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
    fs_inode_unlock(ip);
    ret = console_read(buf, nbyte);
    fs_inode_lock(ip);

    return ret;
  }

  if ((*off > ip->size) || ((*off + nbyte) < *off))
    return -1;

  if ((*off + nbyte) > ip->size)
    nbyte = ip->size - *off;

  if ((ret = ext2_inode_read(ip, buf, nbyte, *off)) < 0)
    return ret;

  ip->atime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  *off += ret;

  return ret;
}

ssize_t
fs_inode_write(struct Inode *ip, const void *buf, size_t nbyte, off_t *off)
{
  ssize_t total;

  if (!kmutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode))
    return console_write(buf, nbyte);

  if ((*off + nbyte) < *off)
    return -1;

  total = ext2_inode_write(ip, buf, nbyte, *off);

  if (total > 0) {
    *off += total;

    if ((size_t) *off > ip->size)
      ip->size = *off;
    
    ip->mtime = rtc_get_time();
    ip->flags |= FS_INODE_DIRTY;
  }

  return total;
}

int
fs_inode_stat(struct Inode *ip, struct stat *buf)
{
  if (!kmutex_holding(&ip->mutex))
    panic("caller not holding ip->mutex");

  buf->st_mode  = ip->mode;
  buf->st_ino   = ip->ino;
  buf->st_dev   = ip->dev;
  buf->st_nlink = ip->nlink;
  buf->st_uid   = ip->uid;
  buf->st_gid   = ip->gid;
  buf->st_size  = ip->size;
  buf->st_atime = ip->atime;
  buf->st_mtime = ip->mtime;
  buf->st_ctime = ip->ctime;

  return 0;
}

int
fs_inode_trunc(struct Inode *ip)
{
  if (!kmutex_holding(&ip->mutex))
    panic("not holding");

  if (!fs_permissions(ip, FS_PERM_WRITE))
    return -EACCESS;

  ext2_inode_trunc(ip);

  ip->size = 0;
  ip->ctime = ip->mtime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  return 0;
}

int
fs_create(const char *path, mode_t mode, dev_t dev, struct Inode **istore)
{
  struct Inode *dp, *ip;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 1, &dp)) < 0)
    return r;

  fs_inode_lock(dp);

  if (ext2_inode_lookup(dp, name) != NULL) {
    r = -EEXISTS;
    goto out;
  }

  mode &= ~process_current()->cmask;

  switch (mode & S_IFMT) {
  case S_IFDIR:
    r = ext2_inode_mkdir(dp, name, mode, &ip);
    break;
  case S_IFREG:
    r = ext2_inode_create(dp, name, mode, &ip);
    break;
  default:
    r = ext2_inode_mknod(dp, name, mode, dev, &ip);
    break;
  }

  // TODO: EROFS

  dp->atime = dp->ctime = dp->mtime = rtc_get_time();
  dp->flags |= FS_INODE_DIRTY;

  ip->ctime = ip->mtime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;
  
  if (r == 0) {
    if (istore == NULL) {
      fs_inode_unlock_put(ip);
    } else {
      *istore = ip;
    }
  }

out:
  fs_inode_unlock_put(dp);

  return r;
}

int
fs_link(char *path1, char *path2)
{
  struct Inode *ip, *dirp;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_name_lookup(path1, &ip)) < 0)
    return r;

  fs_inode_lock(ip);

  if (S_ISDIR(ip->mode)) {
    r = -EPERM;
    goto fail2;
  }

  if (ip->nlink >= LINK_MAX) {
    r = -EMLINK;
    goto fail2;
  }

  ip->nlink++;
  ip->ctime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  // TODO: EROFS

  fs_inode_unlock(ip);

  if ((r = fs_path_lookup(path2, name, 1, &dirp)) < 0)
    goto fail1;

  fs_inode_lock(dirp);

  if (dirp->dev != ip->dev) {
    r = -EXDEV;
    goto fail3;
  }

  if ((r = ext2_inode_link(dirp, name, ip)))
    goto fail3;

  fs_inode_unlock_put(dirp);
  fs_inode_put(ip);

  return 0;

fail3:
  fs_inode_unlock_put(dirp);

  fs_inode_lock(ip);
  ip->nlink--;
  ip->flags |= FS_INODE_DIRTY;
fail2:
  fs_inode_unlock(ip);
fail1:
  fs_inode_put(ip);
  return r;
}

int
fs_unlink(const char *path)
{
  struct Inode *dir, *ip;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 1, &dir)) < 0)
    return r;

  fs_inode_lock(dir);

  if ((ip = ext2_inode_lookup(dir, name)) == NULL) {
    r = -ENOENT;
    goto out1;
  }

  fs_inode_lock(ip);

  if (S_ISDIR(ip->mode)) {
    r = -EPERM;
    goto out2;
  }
  
  if ((r = ext2_inode_unlink(dir, ip)) < 0)
    goto out2;

  if (--ip->nlink > 0)
    ip->ctime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

out2:
  fs_inode_unlock_put(dir);
out1:
  fs_inode_unlock_put(ip);

  return r;
}

int
fs_rmdir(const char *path)
{
  struct Inode *dir;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 1, &dir)) < 0)
    return r;

  return ext2_inode_rmdir(dir, name);
}

int
fs_permissions(struct Inode *inode, mode_t mode)
{
  struct Process *proc = process_current();

  if (proc->uid == inode->uid)
    mode <<= 6;
  else if (proc->gid == inode->gid)
    mode <<= 3;

  return (inode->mode & mode) == mode;
}

int
fs_chdir(struct Inode *ip)
{
  struct Process *current = process_current();

  fs_inode_lock(ip);

  if (!S_ISDIR(ip->mode)) {
    fs_inode_unlock_put(ip);
    return -ENOTDIR;
  }

  fs_inode_unlock(ip);

  fs_inode_put(current->cwd);
  current->cwd = ip;

  return 0;
}

int
fs_chmod(struct Inode *ip, mode_t mode)
{
  struct Process *current = process_current();

  // TODO: check mode
  
  fs_inode_lock(ip);

  if ((current->uid != 0) && (ip->uid != current->uid)) {
    fs_inode_unlock(ip);
    return -EPERM;
  }

  // TODO: additional permission checks

  ip->mode  = mode;
  ip->ctime = rtc_get_time();

  ip->flags |= FS_INODE_DIRTY;
  fs_inode_unlock(ip);

  return 0;
}

