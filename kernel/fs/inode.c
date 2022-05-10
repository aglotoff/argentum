#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/ext2.h>
#include <kernel/fs/fs.h>
#include <kernel/process.h>
#include <kernel/types.h>

static unsigned ext2_inode_block_map(struct Inode *, unsigned);

/*
 * ----------------------------------------------------------------------------
 * Allocating Inodes
 * ----------------------------------------------------------------------------
 */

// Try to allocate an inode from the block group descriptor pointed to by `gd`.
// If there is a free inode, mark it as used and store its number into the
// memory location pointed to by `istore`. Otherwise, return `-ENOMEM`.
static int
ext2_gd_inode_alloc(struct Ext2GroupDesc *gd, dev_t dev, uint32_t *istore)
{
  if (gd->free_inodes_count == 0)
    return -ENOMEM;

  if ((ext2_bmap_alloc(gd->inode_bitmap, sb.inodes_per_group, dev, istore)))
    // If free_inodes_count isn't zero, but we cannot find a free inode, the
    // filesystem is corrupted.
    panic("no free inodes");

  gd->free_inodes_count--;

  return 0;
}

/**
 * Allocate an inode.
 * 
 * @param dev    The device to allocate inode from.
 * @param istore Pointer to the memory location where to store the allocated
 *               inode number.
 *
 * @retval 0       Success
 * @retval -ENOMEM Couldn't find a free inode.
 */
int
ext2_inode_alloc(mode_t mode, dev_t dev, uint32_t *istore, uint32_t parent)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t inum;

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  // First try to find a free block in the same group as the specified inode
  g  = (parent - 1) / sb.inodes_per_group;
  gi = g % gds_per_block;
  g  = g - gi;

  if ((buf = buf_read(2 + (g / gds_per_block), dev)) == NULL)
    panic("cannot read the group descriptor table");

  gd = (struct Ext2GroupDesc *) buf->data + gi;

  if (ext2_gd_inode_alloc(gd, dev, &inum) == 0) {
    unsigned inodes_per_block, itab_idx, inode_block_idx, inode_block;
    struct Ext2Inode *dp;

    inum += 1 + (g + gi) * sb.inodes_per_group;

    inodes_per_block = BLOCK_SIZE / sb.inode_size;
    itab_idx  = (inum - 1) % sb.inodes_per_group;
    inode_block      = gd->inode_table + itab_idx / inodes_per_block;
    inode_block_idx  = itab_idx % inodes_per_block;

    buf_write(buf);
    buf_release(buf);

    if ((buf = buf_read(inode_block, 0)) == NULL)
      panic("cannot read the inode table");

    dp = (struct Ext2Inode *) &buf->data[sb.inode_size * inode_block_idx];
    memset(dp, 0, sb.inode_size);
    dp->mode  = mode;
    dp->ctime = dp->atime = dp->mtime = rtc_time();

    buf_write(buf);
    buf_release(buf);

    if (istore)
      *istore = inum;

    return 0;
  }

  // Scan all group descriptors for a free inode
  for (g = 0; g < sb.inodes_count / sb.inodes_per_group; g += gds_per_block) {
    if ((buf = buf_read(2 + (g / gds_per_block), 0)) == NULL)
      panic("cannot read the group descriptor table");

    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2GroupDesc *) buf->data + gi;
      if (ext2_gd_inode_alloc(gd, dev, &inum) == 0) {
        unsigned inodes_per_block, itab_idx, inode_block_idx, inode_block;
        struct Ext2Inode *dp;

        inum += 1 + (g + gi) * sb.inodes_per_group;

        inodes_per_block = BLOCK_SIZE / sb.inode_size;
        itab_idx  = (inum - 1) % sb.inodes_per_group;
        inode_block      = gd->inode_table + itab_idx / inodes_per_block;
        inode_block_idx  = itab_idx % inodes_per_block;

        buf_write(buf);
        buf_release(buf);

        if ((buf = buf_read(inode_block, 0)) == NULL)
          panic("cannot read the inode table");

        dp = (struct Ext2Inode *) &buf->data[sb.inode_size * inode_block_idx];
        memset(dp, 0, sb.inode_size);
        dp->mode  = mode;
        dp->ctime = dp->atime = dp->mtime = rtc_time();

        buf_write(buf);
        buf_release(buf);

        if (istore)
          *istore = inum;

        return 0;
      }
    }

    buf_release(buf);
  }
  
  return -ENOMEM;
}

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
    mutex_init(&ip->mutex, "inode");
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
    empty->valid     = 0;

    spin_unlock(&inode_cache.lock);

    return empty;
  }

  spin_unlock(&inode_cache.lock);

  return NULL;
}

void
fs_inode_put(struct Inode *ip)
{   
  fs_inode_lock(ip);

  if (ip->nlink == 0) {
    int r;

    spin_lock(&inode_cache.lock);
    r = ip->ref_count;
    spin_unlock(&inode_cache.lock);

    if (r == 1)
      fs_inode_trunc(ip);
  }

  fs_inode_unlock(ip);

  spin_lock(&inode_cache.lock);

  if (--ip->ref_count == 0) {
    list_remove(&ip->cache_link);
    list_add_front(&inode_cache.head, &ip->cache_link);
  }

  spin_unlock(&inode_cache.lock);
}

void
ext2_write_inode(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2GroupDesc gd;
  struct Ext2Inode *dp;
  unsigned gds_per_block, inodes_per_block;
  unsigned block_group, table_block, table_idx;
  unsigned inode_table_idx, inode_block_idx, inode_block;

  // Determine which block group the inode belongs to
  block_group = (ip->ino - 1) / sb.inodes_per_group;

  // Read the Block Group Descriptor corresponding to the Block Group which
  // contains the inode to be looked up
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  table_block = 2 + (block_group / gds_per_block);
  table_idx = (block_group % gds_per_block);

  if ((buf = buf_read(table_block, ip->dev)) == NULL)
    panic("cannot read the group descriptor table");
  memcpy(&gd, &buf->data[sizeof(gd) * table_idx], sizeof(gd));
  buf_release(buf);

  // From the Block Group Descriptor, extract the location of the block
  // group's inode table

  // Determine the index of the inode in the inode table. 
  inodes_per_block = BLOCK_SIZE / sb.inode_size;
  inode_table_idx  = (ip->ino - 1) % sb.inodes_per_group;
  inode_block      = gd.inode_table + inode_table_idx / inodes_per_block;
  inode_block_idx  = inode_table_idx % inodes_per_block;

  // Index the inode table (taking into account non-standard inode size)
  if ((buf = buf_read(inode_block, ip->dev)) == NULL)
    panic("cannot read the inode table");

  dp = (struct Ext2Inode *) &buf->data[inode_block_idx * sb.inode_size];

  dp->mode        = ip->mode;
  dp->links_count = ip->nlink;
  dp->uid         = ip->uid;
  dp->gid         = ip->gid;
  dp->size        = ip->size;
  dp->atime       = ip->atime;
  dp->mtime       = ip->mtime;
  dp->ctime       = ip->ctime;
  dp->blocks      = ip->blocks;
  memmove(dp->block, ip->block, sizeof(ip->block));

  buf_write(buf);
  buf_release(buf);
}

void
ext2_inode_lock(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2GroupDesc gd;
  struct Ext2Inode *dp;
  unsigned gds_per_block, inodes_per_block;
  unsigned block_group, table_block, table_idx;
  unsigned inode_table_idx, inode_block_idx, inode_block;

  // Determine which block group the inode belongs to
  block_group = (ip->ino - 1) / sb.inodes_per_group;

  // Read the Block Group Descriptor corresponding to the Block Group which
  // contains the inode to be looked up
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  table_block = 2 + (block_group / gds_per_block);
  table_idx = (block_group % gds_per_block);

  if ((buf = buf_read(table_block, ip->dev)) == NULL)
    panic("cannot read the group descriptor table");
  memcpy(&gd, &buf->data[sizeof(gd) * table_idx], sizeof(gd));
  buf_release(buf);

  // From the Block Group Descriptor, extract the location of the block
  // group's inode table

  // Determine the index of the inode in the inode table. 
  inodes_per_block = BLOCK_SIZE / sb.inode_size;
  inode_table_idx  = (ip->ino - 1) % sb.inodes_per_group;
  inode_block      = gd.inode_table + inode_table_idx / inodes_per_block;
  inode_block_idx  = inode_table_idx % inodes_per_block;

  // Index the inode table (taking into account non-standard inode size)
  if ((buf = buf_read(inode_block, ip->dev)) == NULL)
    panic("cannot read the inode table");

  dp = (struct Ext2Inode *) (buf->data + inode_block_idx * sb.inode_size);

  ip->mode   = dp->mode;
  ip->nlink  = dp->links_count;
  ip->uid    = dp->uid;
  ip->gid    = dp->gid;
  ip->size   = dp->size;
  ip->atime  = dp->atime;
  ip->mtime  = dp->mtime;
  ip->ctime  = dp->ctime;
  ip->blocks = dp->blocks;
  memmove(ip->block, dp->block, sizeof(ip->block));

  buf_release(buf);

  if (((ip->mode & EXT2_S_IFMASK) == EXT2_S_IFBLK) ||
      ((ip->mode & EXT2_S_IFMASK) == EXT2_S_IFCHR)) {
    if ((buf = buf_read(ext2_inode_block_map(ip, 0), ip->dev)) == NULL)
      panic("cannot read the data block");

    ip->rdev = *(uint16_t *) buf->data;

    buf_release(buf);
  }
}

static struct Inode *
fs_inode_alloc(mode_t mode, dev_t dev, ino_t parent)
{
  struct Inode *ip;
  uint32_t inum;

  if (ext2_inode_alloc(mode, dev, &inum, parent) < 0)
    return NULL;

  if ((ip = fs_inode_get(inum, dev)) == NULL)
    panic("cannot get inode (%u)", inum);

  ip->mode = mode;

  return ip;
}

void
fs_write_inode(struct Inode *ip)
{
  if (!mutex_holding(&ip->mutex))
    panic("caller must hold ip");

  ext2_write_inode(ip);
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

static void
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
  
  if (!mutex_holding(&ip->mutex))
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

  ip->atime = rtc_time();
  fs_write_inode(ip);

  *off += ret;

  return ret;
}

ssize_t
fs_inode_write(struct Inode *ip, const void *buf, size_t nbyte, off_t *off)
{
  ssize_t total;

  if (!mutex_holding(&ip->mutex))
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
    
    ip->mtime = rtc_time();

    fs_write_inode(ip);
  }

  return total;
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

void
fs_inode_trunc(struct Inode *ip)
{
  if (!mutex_holding(&ip->mutex))
    panic("not holding");

  ext2_inode_trunc(ip);
  ip->size = 0;
  fs_write_inode(ip);
}

int
ext2_create(struct Inode *dirp, char *name, mode_t mode, struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((ip = fs_inode_alloc(mode, dirp->dev, dirp->ino)) == NULL)
    return -ENOMEM;

  fs_inode_lock(ip);

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
  ext2_write_inode(ip);

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

  if ((r = ext2_create(dirp, name, mode, &ip)) != 0)
    return r;

  // Create the "." entry
  if (ext2_inode_link(ip, ".", ip) < 0)
    panic("Cannot create .");

  ip->nlink = 1;
  ext2_write_inode(ip);

  // Create the ".." entry
  if (ext2_inode_link(ip, "..", dirp) < 0)
    panic("Cannot create ..");
  dirp->nlink++;
  ext2_write_inode(dirp);

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

  if (S_ISBLK(mode) || S_ISCHR(mode)) {
    ip->rdev = dev;
    ip->size = sizeof(dev);

    ext2_inode_write(ip, &dev, sizeof(dev), 0);
  }

  ext2_write_inode(ip);

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

    ip->nlink--;

    if (S_ISDIR(ip->mode))
      dir->nlink--;

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

  ext2_write_inode(ip);
  ext2_write_inode(dir);

out2:
  fs_inode_unlock_put(ip);

out1:
  fs_inode_unlock_put(dir);

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
    r = -EISDIR;
    goto out2;
  }
  
  if ((r = ext2_inode_unlink(dir, ip)) < 0)
    goto out2;

  fs_write_inode(ip);

out2:
  fs_inode_unlock_put(ip);
out1:
  fs_inode_unlock_put(dir);

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
fs_can_read(struct Inode *inode)
{
  struct Process *current = my_process();

  if (current->uid == inode->uid)
    return inode->mode & S_IRUSR;
  if (current->gid == inode->gid)
    return inode->mode & S_IRGRP;
  return inode->mode & S_IROTH;
}

int
fs_can_write(struct Inode *inode)
{
  struct Process *current = my_process();

  if (current->uid == inode->uid)
    return inode->mode & S_IWUSR;
  if (current->gid == inode->gid)
    return inode->mode & S_IWGRP;
  return inode->mode & S_IWOTH;
}


int
fs_can_exec(struct Inode *inode)
{
  struct Process *current = my_process();

  if (current->uid == inode->uid)
    return inode->mode & S_IXUSR;
  if (current->gid == inode->gid)
    return inode->mode & S_IXGRP;
  return inode->mode & S_IXOTH;
}
