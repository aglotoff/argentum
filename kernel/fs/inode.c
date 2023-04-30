#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <argentum/cprintf.h>
#include <argentum/drivers/console.h>
#include <argentum/drivers/rtc.h>
#include <argentum/fs/buf.h>
#include <argentum/fs/ext2.h>
#include <argentum/fs/fs.h>
#include <argentum/process.h>
#include <argentum/types.h>

static unsigned ext2_inode_block_map(struct Inode *, unsigned);

/*
 * ----------------------------------------------------------------------------
 * Inode Cache
 * ----------------------------------------------------------------------------
 */

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
      ext2_put_inode(ip);
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

static struct Inode *
fs_inode_alloc(mode_t mode, dev_t dev, ino_t parent)
{
  uint32_t inum;

  if (ext2_inode_alloc(mode, dev, &inum, parent) < 0)
    return NULL;

  return fs_inode_get(inum, dev);
}

void
fs_inode_lock(struct Inode *ip)
{
  kmutex_lock(&ip->mutex);

  if (ip->flags & FS_INODE_VALID)
    return;

  if (ip->flags & FS_INODE_DIRTY)
    panic("inode dirty");

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


/*
 * ----------------------------------------------------------------------------
 * Inode Operations
 * ----------------------------------------------------------------------------
 */

#define DIRECT_BLOCKS     12
#define INDIRECT_BLOCKS   BLOCK_SIZE / sizeof(uint32_t)

static unsigned
ext2_inode_block_map(struct Inode *ip, unsigned block_no)
{
  struct Buf *buf;
  uint32_t addr, *a;

  if (block_no < DIRECT_BLOCKS) {
    if ((addr = ip->block[block_no]) == 0) {
      if (ext2_block_alloc(ip->dev, &addr, ip->ino) != 0)
        panic("cannot allocate direct block");
      ip->block[block_no] = addr;
      ip->blocks++;
    }

    return addr;
  }

  block_no -= DIRECT_BLOCKS;

  if (block_no >= INDIRECT_BLOCKS)
    panic("not implemented");
  
  if ((addr = ip->block[DIRECT_BLOCKS]) == 0) {
    if (ext2_block_alloc(ip->dev, &addr, ip->ino) != 0)
      panic("cannot allocate indirect block");
    ip->block[DIRECT_BLOCKS] = addr;
    ip->blocks++;
  }

  if ((buf = buf_read(addr, ip->dev)) == NULL)
    panic("cannot read the block");

  a = (uint32_t *) buf->data;
  if ((addr = a[block_no]) == 0) {
    if (ext2_block_alloc(ip->dev, &addr, ip->ino) != 0)
      panic("cannot allocate indirect block");
    a[block_no] = addr;
    ip->blocks++;
  }

  buf_write(buf);
  buf_release(buf);

  return addr;
}

void
ext2_inode_trunc(struct Inode *ip)
{
  struct Buf *buf;
  uint32_t *a, i;

  for (i = 0; i < DIRECT_BLOCKS; i++) {
    if (ip->block[i] == 0) {
      assert(ip->blocks == 0);
      return;
    }

    ext2_block_free(ip->dev, ip->block[i]);
    ip->block[i] = 0;
    ip->blocks--;
  }

  if (ip->block[DIRECT_BLOCKS] == 0)
    return;
  
  buf = buf_read(ip->block[DIRECT_BLOCKS], ip->dev);
  a = (uint32_t *) buf->data;

  for (i = 0; i < INDIRECT_BLOCKS; i++) {
    if (a[i] == 0)
      break;
    
    ext2_block_free(ip->dev, a[i]);
    a[i] = 0;
    ip->blocks--;
  }

  buf_write(buf);
  buf_release(buf);

  ext2_block_free(ip->dev, ip->block[DIRECT_BLOCKS]);
  ip->block[DIRECT_BLOCKS] = 0;
  ip->blocks--;

  assert(ip->blocks == 0);
}

ssize_t
ext2_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  uint32_t bno;
  size_t total, nread;
  uint8_t *dst;

  dst = (uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    bno = ext2_inode_block_map(ip, off / BLOCK_SIZE);
    if ((b = buf_read(bno, ip->dev)) == NULL)
      panic("cannot read the block");

    nread = MIN(BLOCK_SIZE - (size_t) off % BLOCK_SIZE, nbyte - total);
    memmove(dst, &((const uint8_t *) b->data)[off % BLOCK_SIZE], nread);

    buf_release(b);

    total += nread;
    dst   += nread;
    off   += nread;
  }

  return total;
}

ssize_t
ext2_inode_write(struct Inode *ip, const void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  size_t total, nwrite;
  const uint8_t *src;
  uint32_t bno;

  src = (const uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    bno = ext2_inode_block_map(ip, off / BLOCK_SIZE);
    if ((b = buf_read(bno, ip->dev)) == NULL)
      panic("cannot read the block");

    nwrite = MIN(BLOCK_SIZE - (size_t) off % BLOCK_SIZE, nbyte - total);
    memmove(&((uint8_t *) b->data)[off % BLOCK_SIZE], src, nwrite);

    buf_write(b);
    buf_release(b);

    total += nwrite;
    src   += nwrite;
    off   += nwrite;
  }

  return total;
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
ext2_create(struct Inode *dirp, char *name, mode_t mode, struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((ip = fs_inode_alloc(mode, dirp->dev, dirp->ino)) == NULL)
    return -ENOMEM;

  fs_inode_lock(ip);

  ip->uid = process_current()->uid;
  ip->gid = dirp->gid;

  if ((r = ext2_inode_link(dirp, name, ip)))
    panic("Cannot create link");

  *istore = ip;

  return 0;
}

int
ext2_inode_create(struct Inode *dirp, char *name, mode_t mode,
                  struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_create(dirp, name, mode, &ip)) != 0)
    return r;

  ip->nlink = 1;
  ip->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;

  return 0;
}

int
ext2_inode_mkdir(struct Inode *dirp, char *name, mode_t mode,
                 struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if (dirp->nlink >= LINK_MAX)
    return -EMLINK;

  if ((r = ext2_create(dirp, name, mode, &ip)) != 0)
    return r;

  // Create the "." entry
  if (ext2_inode_link(ip, ".", ip) < 0)
    panic("Cannot create .");

  ip->nlink = 1;
  ip->flags |= FS_INODE_DIRTY;

  // Create the ".." entry
  if (ext2_inode_link(ip, "..", dirp) < 0)
    panic("Cannot create ..");
  dirp->nlink++;
  dirp->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;
  
  return 0;
}

int
ext2_inode_mknod(struct Inode *dirp, char *name, mode_t mode, dev_t dev,
                 struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_create(dirp, name, mode, &ip)) != 0)
    return r;

  ip->nlink = 1;
  ip->rdev = dev;
  ip->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;

  return 0;
}

#define DE_NAME_OFFSET    offsetof(struct Ext2DirEntry, name)

struct Inode *
ext2_inode_lookup(struct Inode *dirp, const char *name)
{
  struct Ext2DirEntry de;
  off_t off;
  size_t name_len;

  if (!S_ISDIR(dirp->mode))
    panic("not a directory");

  name_len = strlen(name);

  for (off = 0; off < dirp->size; off += de.rec_len) {
    ext2_dirent_read(dirp, &de, off);

    if (de.inode == 0)
      continue;

    if (de.name_len != name_len)
      continue;

    if (strncmp(de.name, name, de.name_len) == 0)
      return fs_inode_get(de.inode, 0);
  }

  return NULL;
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
ext2_inode_link(struct Inode *dir, char *name, struct Inode *ip)
{
  struct Inode *ip2;
  struct Ext2DirEntry de, new_de;
  off_t off;
  ssize_t name_len, de_len, new_len;
  uint8_t file_type;

  if ((ip2 = ext2_inode_lookup(dir, name)) != NULL) {
    fs_inode_put(ip2);
    return -EEXISTS;
  }

  name_len = strlen(name);
  if (name_len > NAME_MAX)
    return -ENAMETOOLONG;

  switch (ip->mode & S_IFMT) {
  case S_IFREG:
    file_type = EXT2_FT_REG_FILE; break;
  case S_IFSOCK:
    file_type = EXT2_FT_SOCK; break;
  case S_IFBLK:
    file_type = EXT2_FT_BLKDEV; break;
  case S_IFCHR:
    file_type = EXT2_FT_CHRDEV; break;
  case S_IFDIR:
    file_type = EXT2_FT_DIR; break;
  case S_IFIFO:
    file_type = EXT2_FT_FIFO; break;
  case S_IFLNK:
    file_type = EXT2_FT_SYMLINK; break;
  default:
    return -EINVAL;
  }

  new_len = ROUND_UP(DE_NAME_OFFSET + name_len, sizeof(uint32_t));

  new_de.inode     = ip->ino;
  new_de.name_len  = name_len;
  new_de.file_type = file_type;
  strncpy(new_de.name, name, ROUND_UP(name_len, sizeof(uint32_t)));

  for (off = 0; off < dir->size; off += de.rec_len) {
    ext2_dirent_read(dir, &de, off);

    if (de.inode == 0) {
      if (de.rec_len < new_len)
        continue;
      
      // Reuse an empty entry
      new_de.rec_len = de.rec_len;

      return ext2_dirent_write(dir, &new_de, off);
    }

    de_len = ROUND_UP(DE_NAME_OFFSET + de.name_len, sizeof(uint32_t));

    if ((de.rec_len - de_len) >= new_len) {
      // Found enough space
      new_de.rec_len = de.rec_len - de_len;
      de.rec_len = de_len;

      ext2_dirent_write(dir, &de, off);
      ext2_dirent_write(dir, &new_de, off + de_len);

      return 0;
    }
  }

  assert(off % BLOCK_SIZE == 0);

  new_de.rec_len = BLOCK_SIZE;
  dir->size = off + BLOCK_SIZE;

  return ext2_dirent_write(dir, &new_de, off);
}

static int
ext2_dir_empty(struct Inode *dir)
{
  struct Ext2DirEntry de;
  off_t off;

  for (off = 0; off < dir->size; off += de.rec_len) {
    ext2_dirent_read(dir, &de, off);

    if (de.inode == 0)
      continue;

    if ((de.name_len == 1) && (strncmp(de.name, ".", de.name_len) == 0))
      continue;
    if ((de.name_len == 2) && (strncmp(de.name, "..", de.name_len) == 0))
      continue;

    return 0;
  }

  return 1;
}

int
ext2_inode_unlink(struct Inode *dir, struct Inode *ip)
{
  struct Ext2DirEntry de;
  off_t off, prev_off;
  size_t rec_len;

  if (dir->ino == ip->ino)
    return -EBUSY;

  for (prev_off = off = 0; off < dir->size; prev_off = off, off += de.rec_len) {
    ext2_dirent_read(dir, &de, off);

    if (de.inode != ip->ino)
      continue;

    if (prev_off == off) {
      // Removed the first entry - create an unused entry
      memset(de.name, 0, de.name_len);
      de.name_len  = 0;
      de.file_type = 0;
      de.inode     = 0;

      ext2_dirent_write(dir, &de, off);
    } else {
      // Update length of the previous entry
      rec_len = de.rec_len;

      ext2_dirent_read(dir, &de, prev_off);
      de.rec_len += rec_len;
      ext2_dirent_write(dir, &de, prev_off);
    }

    return 0;
  }

  return -ENOENT;
}

int
ext2_inode_rmdir(struct Inode *dir, char *name)
{
  struct Inode *ip;
  int r;

  fs_inode_lock(dir);

  if ((ip = ext2_inode_lookup(dir, name)) == NULL) {
    r = -ENOENT;
    goto out1;
  }

  fs_inode_lock(ip);

  if (!S_ISDIR(ip->mode)) {
    r = -ENOTDIR;
    goto out2;
  }

  if (!ext2_dir_empty(ip)) {
    r = -ENOTEMPTY;
    goto out2;
  }

  if ((r = ext2_inode_unlink(dir, ip)) < 0)
    goto out2;

  if (--ip->nlink > 0)
    ip->ctime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  dir->nlink--;
  dir->ctime = dir->mtime = rtc_get_time();
  dir->flags |= FS_INODE_DIRTY;

out2:
  fs_inode_unlock_put(dir);
out1:
  fs_inode_unlock_put(ip);

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
