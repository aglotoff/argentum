#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/fs/ext2.h>
#include <kernel/mm/kobject.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include <kernel/fs/fs.h>

/*
 * ----------------------------------------------------------------------------
 * Inode operations
 * ----------------------------------------------------------------------------
 */

static struct {
  struct Inode    buf[INODE_CACHE_SIZE];
  struct SpinLock lock;
  struct ListLink head;
} inode_cache;

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
    empty->valid     = 0;

    spin_unlock(&inode_cache.lock);

    return empty;
  }

  spin_unlock(&inode_cache.lock);

  return NULL;
}

struct Inode *
fs_inode_alloc(mode_t mode, dev_t dev)
{
  struct Inode *ip;
  uint32_t inum;

  if (ext2_inode_alloc(mode, &inum) < 0)
    return NULL;

  if ((ip = fs_inode_get(inum, dev)) == NULL)
    panic("cannot get inode (%u)", inum);

  ip->mode = mode;

  return ip;
}

void
fs_inode_update(struct Inode *ip)
{
  if (!mutex_holding(&ip->mutex))
    panic("caller must hold ip");

  ext2_inode_update(ip);
}

void
fs_inode_lock(struct Inode *ip)
{
  mutex_lock(&ip->mutex);

  if (ip->valid)
    return;

  ext2_inode_lock(ip);

  if (!ip->mode)
    panic("no mode");

  ip->valid = 1;
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
  if (!mutex_holding(&ip->mutex))
    panic("not holding buf");

  mutex_unlock(&ip->mutex);
}

void
fs_inode_put(struct Inode *ip)
{   
  spin_lock(&inode_cache.lock);

  assert(ip->ref_count > 0);

  if (--ip->ref_count == 0) {
    list_remove(&ip->cache_link);
    list_add_front(&inode_cache.head, &ip->cache_link);
  }

  spin_unlock(&inode_cache.lock);
}

ssize_t
fs_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  if (!mutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode))
    return console_read(buf, nbyte);

  if ((off > ip->size) || ((off + nbyte) < off))
    return -1;

  if ((off + nbyte) > ip->size)
    nbyte = ip->size - off;

  return ext2_inode_read(ip, buf, nbyte, off);
}

ssize_t
fs_inode_write(struct Inode *ip, const void *buf, size_t nbyte, off_t off)
{
  ssize_t total;

  if (!mutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode))
    return console_write(buf, nbyte);

  if ((off + nbyte) < off)
    return -1;

  total = ext2_inode_write(ip, buf, nbyte, off);

  if ((total > 0) && ((size_t) off + total > ip->size)) {
    ip->size = off + total;
    fs_inode_update(ip);
  }

  return total;
}

ssize_t
fs_inode_getdents(struct Inode *dir, void *buf, size_t n, off_t *off)
{
  ssize_t nread, total;
  char *dst;
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  
  dst = (char *) buf;
  total = 0;
  while ((n > 0) && (nread = ext2_dir_read(dir, dst, n, off)) != 0) {
    if (nread < 0)
      return nread;

    dst   += nread;
    total += nread;
    n     -= nread;
  }

  return total;
}

struct Inode *
fs_dir_lookup(struct Inode *dir, const char *name)
{
  if (!S_ISDIR(dir->mode))
    panic("not a directory");

  return ext2_dir_lookup(dir, name);
}

int
fs_dir_link(struct Inode *dp, char *name, unsigned num, mode_t mode)
{
  struct Inode *ip;

  if ((ip = fs_dir_lookup(dp, name)) != NULL) {
    fs_inode_put(ip);
    return -EEXISTS;
  }

  if (strlen(name) > NAME_MAX)  {
    fs_inode_put(ip);
    return -ENAMETOOLONG;
  }

  return ext2_dir_link(dp, name, num, mode);
}

/*
 * ----------------------------------------------------------------------------
 * Path names operations
 * ----------------------------------------------------------------------------
 */

static size_t
fs_path_skip(const char *path, char *name, char **next)
{
  const char *end;
  ptrdiff_t n;

  // Skip leading slashes
  while (*path == '/')
    path++;
  
  // This was the last element
  if (*path == '\0')
    return 0;

  // Find the end of the element
  for (end = path; (*end != '\0') && (*end != '/'); end++)
    ;

  n = end - path;
  strncpy(name, path, MIN(n, NAME_MAX));
  name[n] = '\0';

  while (*end == '/')
    end++;

  if (next)
    *next = (char *) end;

  return n;
}

int
fs_path_lookup(const char *path, char *name, int parent, struct Inode **istore)
{
  struct Inode *ip, *next;
  size_t namelen;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the current working directory.
  ip = *path == '/' ? fs_inode_get(2, 0) : fs_inode_dup(my_process()->cwd);

  while ((namelen = fs_path_skip(path, name, (char **) &path)) > 0) {
    if (namelen > NAME_MAX) {
      fs_inode_put(ip);
      return -ENAMETOOLONG;
    }

    fs_inode_lock(ip);

    if ((ip->mode & EXT2_S_IFMASK) != EXT2_S_IFDIR) {
      fs_inode_unlock(ip);
      fs_inode_put(ip);
      return -ENOTDIR;
    }

    if (parent && (*path == '\0')) {
      fs_inode_unlock(ip);

      if (istore)
        *istore = ip;
      return 0;
    }

    if ((next = fs_dir_lookup(ip, name)) == NULL) {
      fs_inode_unlock(ip);
      fs_inode_put(ip);
      return -ENOENT;
    }

    fs_inode_unlock(ip);
    fs_inode_put(ip);

    ip = next;
  }

  if (parent) {
    // File exists!
    fs_inode_put(ip);
    return -EEXISTS;
  }

  if (istore)
    *istore = ip;
  return 0;
}

int
fs_name_lookup(const char *path, struct Inode **ip)
{
  char name[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *ip = fs_inode_get(2, 0);
    return 0;
  }

  return fs_path_lookup(path, name, 0, ip);
}

int
fs_inode_stat(struct Inode *ip, struct stat *buf)
{
  if (!mutex_holding(&ip->mutex))
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
fs_create(const char *path, mode_t mode, dev_t dev, struct Inode **istore)
{
  struct Inode *dp, *ip;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 1, &dp)) < 0)
    return r;

  fs_inode_lock(dp);

  if (fs_dir_lookup(dp, name) != NULL) {
    r = -EEXISTS;
    goto out;
  }

  if ((ip = fs_inode_alloc(mode, dp->dev)) == NULL) {
    r = -ENOMEM;
    goto out;
  }

  fs_inode_lock(ip);

  ip->nlink = 1;
  fs_inode_update(ip);

  if (S_ISDIR(mode)) {
    // Create . and .. entries
    dp->nlink++;
    fs_inode_update(dp);

    if (fs_dir_link(ip, ".", ip->ino, EXT2_S_IFDIR) < 0)
      panic("Cannot create .");
    if (fs_dir_link(ip, "..", dp->ino, EXT2_S_IFDIR) < 0)
      panic("Cannot create ..");
  } else if ((S_ISCHR(mode) || S_ISBLK(mode))) {
    ip->major = (dev >> 8) & 0xFF;
    ip->minor = dev & 0xFF;

    ext2_inode_write(ip, &dev, sizeof(dev), 0);
    ip->size = sizeof(dev);

    fs_inode_update(ip);
  }

  r = fs_dir_link(dp, name, ip->ino, mode);

  if ((r == 0) && istore) {
    *istore = ip;
  } else {
    fs_inode_unlock(ip);
    fs_inode_put(ip);
  }

out:
  fs_inode_unlock(dp);
  fs_inode_put(dp);

  return r;
}

void
fs_init(void)
{
  struct Inode *ip;
  
  spin_init(&inode_cache.lock, "inode_cache");
  list_init(&inode_cache.head);

  for (ip = inode_cache.buf; ip < &inode_cache.buf[INODE_CACHE_SIZE]; ip++) {
    mutex_init(&ip->mutex, "inode");
    list_init(&ip->wait_queue);

    list_add_back(&inode_cache.head, &ip->cache_link);
  }

  ext2_read_superblock();
}
