#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/ext2.h>
#include <kernel/fs/fs.h>
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
fs_gd_inode_alloc(struct Ext2GroupDesc *gd, uint32_t *istore)
{
  uint32_t b, bi;

  if (gd->free_inodes_count == 0)
    return -ENOMEM;

  for (b = 0; b < sb.blocks_per_group; b += BITS_PER_BLOCK) {
    struct Buf *buf;
    uint32_t *map;

    if ((buf = buf_read(gd->inode_bitmap + b, 0)) == NULL)
      panic("cannot read the bitmap block");

    map = (uint32_t *) buf->data;

    for (bi = 0; bi < BITS_PER_BLOCK; bi++) {
      if (map[bi / 32] & (1 << (bi % 32)))
        continue;

      map[bi / 32] |= (1 << (bi % 32));  
      gd->free_inodes_count--;

      buf_write(buf);
      buf_release(buf);

      *istore = b + bi;

      return 0;
    }

    buf_release(buf);
  }

  // If free_inodes_count isn't zero, but we cannot find a free inode, the
  // filesystem is corrupted.
  panic("cannot allocate inode");
  return -ENOMEM;
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
ext2_inode_alloc(mode_t mode, uint32_t *istore)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t inum;

  // TODO: First try to allocate a new inode in the same group as its parent

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  for (g = 0; g < sb.inodes_count / sb.inodes_per_group; g += gds_per_block) {
    if ((buf = buf_read(2 + (g / gds_per_block), 0)) == NULL)
      panic("cannot read the group descriptor table");

    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2GroupDesc *) buf->data + gi;
      if (fs_gd_inode_alloc(gd, &inum) == 0) {
        unsigned inodes_per_block, itab_idx, inode_block_idx, inode_block;
        struct Ext2Inode *dp;

        inum += 1 + (g + gi) * sb.inodes_per_group;

        inodes_per_block = BLOCK_SIZE / sb.inode_size;
        itab_idx  = (inum - 1) % sb.inodes_per_group;
        inode_block      = gd->inode_table + itab_idx / inodes_per_block;
        inode_block_idx  = itab_idx % inodes_per_block;

        buf_release(buf);

        if ((buf = buf_read(inode_block, 0)) == NULL)
          panic("cannot read the inode table");

        dp = (struct Ext2Inode *) &buf->data[sb.inode_size * inode_block_idx];
        memset(dp, 0, sb.inode_size);
        dp->mode = mode;

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
  spin_lock(&inode_cache.lock);

  assert(ip->ref_count > 0);

  if (--ip->ref_count == 0) {
    list_remove(&ip->cache_link);
    list_add_front(&inode_cache.head, &ip->cache_link);
  }

  spin_unlock(&inode_cache.lock);
}

void
ext2_inode_update(struct Inode *ip)
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

  if (((ip->mode & EXT2_S_IFMASK) == EXT2_S_IFCHR) ||
      ((ip->mode & EXT2_S_IFMASK) == EXT2_S_IFCHR)) {
    uint16_t dev;

    if ((buf = buf_read(ext2_inode_block_map(ip, 0), ip->dev)) == NULL)
      panic("cannot read the data block");
    dev = *(uint16_t *) buf->data;
    buf_release(buf);
  
    ip->major = (dev >> 8) & 0xFF;
    ip->minor = dev & 0xFF;
  }
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


/*
 * ----------------------------------------------------------------------------
 * Inode Contents
 * ----------------------------------------------------------------------------
 */

#define ADDRS_PER_BLOCK BLOCK_SIZE / sizeof(uint32_t)

static unsigned
ext2_inode_block_map(struct Inode *ip, unsigned block_no)
{
  struct Buf *buf;
  uint32_t addr, *ptr;
  unsigned bcnt;

  if (block_no < 12) {
    if ((addr = ip->block[block_no]) == 0) {
      if (fs_block_alloc(ip->dev, &addr) != 0)
        panic("cannot allocate block");
      ip->block[block_no] = addr;
    }
    return addr;
  }

  block_no -= 12;

  ptr = &ip->block[12];
  for (bcnt = ADDRS_PER_BLOCK; bcnt <= block_no; bcnt *= ADDRS_PER_BLOCK) {
    if (++ptr >= &ip->block[15])
      panic("too large block number (%u)", block_no + 12);
  }

  if ((addr = *ptr) == 0) {
    if (fs_block_alloc(ip->dev, &addr) != 0)
      panic("cannot allocate block");
    *ptr = addr;
  }

  while ((bcnt /= ADDRS_PER_BLOCK) > 0) {
    if ((buf = buf_read(addr, ip->dev)) == NULL)
      panic("cannot read the data block");

    ptr = (uint32_t *) buf->data;
    if ((addr = ptr[block_no / bcnt]) == 0) {
      if (fs_block_alloc(ip->dev, &addr) != 0)
        panic("cannot allocate block");
      ptr[block_no / bcnt] = addr;
      buf_write(buf);
    }

    buf_release(buf);

    block_no %= bcnt;
  }

  return addr;
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
fs_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  if (!mutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
    ssize_t ret;

    fs_inode_unlock(ip);
    ret = console_read(buf, nbyte);
    fs_inode_lock(ip);

    return ret;
  }

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

/*
 * ----------------------------------------------------------------------------
 * Inode Cache
 * ----------------------------------------------------------------------------
 */

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
