#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <argentum/cprintf.h>
#include <argentum/drivers/rtc.h>
#include <argentum/fs/buf.h>
#include <argentum/fs/fs.h>
#include <argentum/process.h>
#include <argentum/types.h>

#include "ext2.h"

// The number of bits per bitmap block
// TODO: what about different block sizes?
#define BITS_PER_BLOCK   (BLOCK_SIZE * 8)

/*
 * ----------------------------------------------------------------------------
 * Bitmaps
 * ----------------------------------------------------------------------------
 */

/**
 * Try to allocate a bit from the bitmap.
 * 
 * @param bitmap Starting block number of the bitmap.
 * @param n      The length of the bitmap (in bits).
 * @param dev    The device where the bitmap is located.
 * @param bstore Pointer to the memory location to store the relative number of
 *               the allocated bit.
 * 
 * @return 0 on success, -ENOMEM if there are no unused bits.
 */
int
ext2_bitmap_alloc(uint32_t start, size_t n, dev_t dev, uint32_t *bstore)
{
  uint32_t b, bi;

  for (b = 0; b < n; b++) {
    struct Buf *buf;
    uint32_t *bmap;

    if ((buf = buf_read(start + b, dev)) == NULL)
      panic("cannot read the bitmap block");

    bmap = (uint32_t *) buf->data;

    for (bi = 0; bi < BITS_PER_BLOCK; bi++) {
      if (bmap[bi / 32] & (1 << (bi % 32)))
        continue;

      bmap[bi / 32] |= (1 << (bi % 32));  

      buf_write(buf);
      buf_release(buf);

      *bstore = (b * BITS_PER_BLOCK) + bi;

      return 0;
    }

    buf_release(buf);
  }

  return -ENOMEM;
}

/**
 * Free the allocated bit.
 * 
 * @param bitmap Starting block number of the bitmap.
 * @param dev    The device where the bitmap is located.
 * @param bit_no The bit number to be freed.
 */
void
ext2_bitmap_free(uint32_t bitmap, dev_t dev, uint32_t bit_no)
{
  uint32_t b, bi;
  struct Buf *buf;
  uint32_t *map;

  b  = bit_no / BITS_PER_BLOCK;
  bi = bit_no % BITS_PER_BLOCK;

  if ((buf = buf_read(bitmap + b, dev)) == NULL)
    panic("cannot read the bitmap block");

  map = (uint32_t *) buf->data;

  if (!(map[bi / 32] & (1 << (bi % 32))))
    panic("not allocated");

  map[bi / 32] &= ~(1 << (bi % 32));

  buf_write(buf);
  buf_release(buf);
}

/*
 * ----------------------------------------------------------------------------
 * Block allocator
 * ----------------------------------------------------------------------------
 */

// Block descriptors begin at block 2
#define GD_BLOCKS_BASE  2

/**
 * Fill the block with zeros,
 * 
 * @param block_no The block number.
 * @param dev      The device.
 */
void
ext2_block_zero(uint32_t block_no, uint32_t dev)
{
  struct Buf *buf;
  
  if ((buf = buf_read(block_no, dev)) == NULL)
    panic("Cannot read block");

  memset(buf->data, 0, BLOCK_SIZE);

  buf_write(buf);
  buf_release(buf);
}

// Try to allocate a block from the block group descriptor pointed to by `gd`.
// If there is a free block, mark it as used and store its number (relative to
// the group) into the memory location pointed to by `bstore`. Otherwise,
// return `-ENOMEM`.
static int
ext2_block_group_alloc(struct Ext2GroupDesc *gd, dev_t dev, uint32_t *bstore)
{
  if (gd->free_blocks_count == 0)
    return -ENOMEM;

  if (ext2_bitmap_alloc(gd->block_bitmap, sb.blocks_per_group, dev, bstore) < 0)
    // If free_blocks_count isn't zero, but we couldn't find a free block, the
    // filesystem is corrupted.
    panic("no free blocks");

  gd->free_blocks_count--;

  return 0;
}

/**
 * Allocate a zeroed block.
 * 
 * @param dev    The device to allocate block from.
 * @param bstore Pointer to the memory location where to store the allocated
 *               block number.
 * @param inode  The number of the inode for which the allocation is performed
 *               (used as a hint where to begin the search).
 *
 * @retval 0       Success
 * @retval -ENOMEM Couldn't find a free block.
 */
int
ext2_block_alloc(dev_t dev, uint32_t *bstore, uint32_t ino)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gdesc;
  uint32_t gds_per_block, g, gi;
  uint32_t block_no;

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  // First try to find a free block in the same group as the specified inode
  g  = (ino - 1) / sb.inodes_per_group;
  gi = g % gds_per_block;
  g  = g - gi;

  if ((buf = buf_read(GD_BLOCKS_BASE + (g / gds_per_block), dev)) == NULL)
    panic("cannot read the group descriptor table");

  gdesc = (struct Ext2GroupDesc *) buf->data + gi;

  if (ext2_block_group_alloc(gdesc, dev, &block_no) == 0) {
    buf_write(buf);
    buf_release(buf);

    block_no += (g + gi) * sb.blocks_per_group;

    ext2_block_zero(block_no, dev);

    *bstore = block_no;
    return 0;
  }

  // Scan all group descriptors for a free block
  for (g = 0; g < sb.block_count / sb.blocks_per_group; g += gds_per_block) {
    if ((buf = buf_read(GD_BLOCKS_BASE + (g / gds_per_block), dev)) == NULL)
      panic("cannot read the group descriptor table");

    for (gi = 0; gi < gds_per_block; gi++) {
      gdesc = (struct Ext2GroupDesc *) buf->data + gi;

      if (ext2_block_group_alloc(gdesc, dev, &block_no) == 0) {
        buf_write(buf);
        buf_release(buf);

        block_no += (g + gi) * sb.blocks_per_group;

        ext2_block_zero(block_no, dev);

        *bstore = block_no;
        return 0;
      }
    }

    buf_release(buf);
  }
  
  return -ENOMEM;
}

/**
 * Free a disk block.
 * 
 * @param dev The device the block belongs to.
 * @param bno The block number.
 */
void
ext2_block_free(dev_t dev, uint32_t bno)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, gd_idx, g, gi;

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  gd_idx = bno / sb.blocks_per_group;
  g  = gd_idx / gds_per_block;
  gi = gd_idx % gds_per_block;

  buf = buf_read(2 + g, dev);

  gd = (struct Ext2GroupDesc *) buf->data + gi;

  ext2_bitmap_free(gd->block_bitmap, dev, bno % sb.blocks_per_group);

  gd->free_blocks_count++;

  buf_write(buf);
  buf_release(buf);
}

/*
 * ----------------------------------------------------------------------------
 * Inode allocator
 * ----------------------------------------------------------------------------
 */

// Try to allocate an inode from the inode group descriptor pointed to by `gd`.
// If there is a free inode, mark it as used and store its number into the
// memory location pointed to by `istore`. Otherwise, return `-ENOMEM`.
static int
ext2_inode_group_alloc(struct Ext2GroupDesc *gd, dev_t dev, uint32_t *istore)
{
  if (gd->free_inodes_count == 0)
    return -ENOMEM;

  if (ext2_bitmap_alloc(gd->inode_bitmap, sb.inodes_per_group, dev, istore))
    // If free_inodes_count isn't zero, but we cannot find a free inode, the
    // filesystem is corrupted.
    panic("no free inodes");

  gd->free_inodes_count--;

  return 0;
}

static int
ext2_inode_init(uint32_t table, uint32_t inum, uint16_t mode)
{
  unsigned inodes_per_block, itab_idx, inode_block_idx, inode_block;
  struct Ext2Inode *dp;
  struct Buf *buf;

  inodes_per_block = BLOCK_SIZE / sb.inode_size;
  itab_idx  = (inum - 1) % sb.inodes_per_group;
  inode_block      = table + itab_idx / inodes_per_block;
  inode_block_idx  = itab_idx % inodes_per_block;

  if ((buf = buf_read(inode_block, 0)) == NULL)
    panic("cannot read the inode table");

  dp = (struct Ext2Inode *) &buf->data[sb.inode_size * inode_block_idx];
  memset(dp, 0, sb.inode_size);
  dp->mode  = mode;
  dp->ctime = dp->atime = dp->mtime = rtc_get_time();

  buf_write(buf);
  buf_release(buf);

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

  if (ext2_inode_group_alloc(gd, dev, &inum) == 0) {
    uint32_t table;

    table = gd->inode_table;

    buf_write(buf);
    buf_release(buf);

    inum += 1 + (g + gi) * sb.inodes_per_group;

    ext2_inode_init(table, inum, mode);

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
      if (ext2_inode_group_alloc(gd, dev, &inum) == 0) {
        uint32_t table;

        table = gd->inode_table;

        buf_write(buf);
        buf_release(buf);

        inum += 1 + (g + gi) * sb.inodes_per_group;

        ext2_inode_init(table, inum, mode);

        if (istore)
          *istore = inum;

        return 0;
      }
    }

    buf_release(buf);
  }
  
  return -ENOMEM;
}

/**
 * Free a disk inode.
 * 
 * @param dev The device the inode belongs to.
 * @param bno The inode number.
 */
void
ext2_inode_free(dev_t dev, uint32_t ino)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, gd_idx, g, gi;

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  gd_idx = ino / sb.inodes_per_group;
  g  = gd_idx / gds_per_block;
  gi = gd_idx % gds_per_block;

  buf = buf_read(2 + g, dev);

  gd = (struct Ext2GroupDesc *) buf->data + gi;

  ext2_bitmap_free(gd->inode_bitmap, dev, (ino - 1) % sb.inodes_per_group);

  gd->free_inodes_count++;

  buf_write(buf);
  buf_release(buf);
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

  ip->size = 0;
  ip->ctime = ip->mtime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;
}

ssize_t
ext2_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  uint32_t bno;
  size_t total, nread;
  uint8_t *dst;

  if (off > ip->size)
    return -EINVAL;

  if ((off + nbyte) > ip->size)
    nbyte = ip->size - off;

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

  ip->atime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

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

  if (total > 0) {
    if (off > ip->size)
      ip->size = off;

    ip->mtime = rtc_get_time();
    ip->flags |= FS_INODE_DIRTY;
  }

  return total;
}

static struct Inode *
fs_inode_alloc(mode_t mode, dev_t dev, ino_t parent)
{
  uint32_t inum;

  if (ext2_inode_alloc(mode, dev, &inum, parent) < 0)
    return NULL;

  return fs_inode_get(inum, dev);
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

  ip->ctime = ip->mtime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

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

  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
  dirp->flags |= FS_INODE_DIRTY;

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
  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
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

  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
  dirp->flags |= FS_INODE_DIRTY;

  return 0;
}

#define DE_NAME_OFFSET    offsetof(struct Ext2DirEntry, name)

ssize_t
ext2_dirent_read(struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  ssize_t ret;

  ret = ext2_inode_read(dir, de, DE_NAME_OFFSET, off);
  if (ret != DE_NAME_OFFSET)
    panic("Cannot read directory");

  ret = ext2_inode_read(dir, de->name, de->name_len, off + ret);
  if (ret != de->name_len)
    panic("Cannot read directory");

  return 0;
}

ssize_t
ext2_dirent_write(struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  size_t ret;

  ret = ext2_inode_write(dir, de, DE_NAME_OFFSET + de->name_len, off);
  if (ret != (DE_NAME_OFFSET + de->name_len))
    panic("Cannot read directory");

  return 0;
}

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

      ip->ctime = rtc_get_time();
      ip->nlink++;
      ip->flags |= FS_INODE_DIRTY;

      return ext2_dirent_write(dir, &new_de, off);
    }

    de_len = ROUND_UP(DE_NAME_OFFSET + de.name_len, sizeof(uint32_t));

    if ((de.rec_len - de_len) >= new_len) {
      // Found enough space
      new_de.rec_len = de.rec_len - de_len;
      de.rec_len = de_len;

      ip->ctime = rtc_get_time();
      ip->nlink++;
      ip->flags |= FS_INODE_DIRTY;

      ext2_dirent_write(dir, &de, off);
      ext2_dirent_write(dir, &new_de, off + de_len);

      return 0;
    }
  }

  assert(off % BLOCK_SIZE == 0);

  new_de.rec_len = BLOCK_SIZE;
  dir->size = off + BLOCK_SIZE;

  ip->ctime = rtc_get_time();
  ip->nlink++;
  ip->flags |= FS_INODE_DIRTY;

  ext2_dirent_write(dir, &new_de, off);

  return 0;
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

    if (--ip->nlink > 0)
      ip->ctime = rtc_get_time();
    ip->flags |= FS_INODE_DIRTY;

    return 0;
  }

  return -ENOENT;
}

int
ext2_inode_rmdir(struct Inode *dir, struct Inode *ip)
{
  int r;
  
  if (!ext2_dir_empty(ip))
    return -ENOTEMPTY;

  if ((r = ext2_inode_unlink(dir, ip)) < 0)
    return r;

  dir->nlink--;
  dir->ctime = dir->mtime = rtc_get_time();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

static uint32_t
ext2_get_inode_block(struct Inode *ip, uint32_t *idx)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  unsigned gds_per_block, inodes_per_block;
  unsigned block_group, table_block, table_idx;
  unsigned inode_table_idx, block;

  // Determine which block group the inode belongs to
  block_group = (ip->ino - 1) / sb.inodes_per_group;

  // Read the Block Group Descriptor corresponding to the Block Group which
  // contains the inode to be looked up
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  table_block = 2 + (block_group / gds_per_block);
  table_idx = (block_group % gds_per_block);

  if ((buf = buf_read(table_block, ip->dev)) == NULL)
    panic("cannot read the group descriptor table");

  gd = (struct Ext2GroupDesc *) buf->data + table_idx;

  // From the Block Group Descriptor, extract the location of the block
  // group's inode table

  // Determine the index of the inode in the inode table. 
  inodes_per_block = BLOCK_SIZE / sb.inode_size;
  inode_table_idx  = (ip->ino - 1) % sb.inodes_per_group;

  *idx = inode_table_idx % inodes_per_block;

  block = gd->inode_table + inode_table_idx / inodes_per_block;

  buf_release(buf);

  return block;
}

void
ext2_read_inode(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2Inode *dp;
  unsigned inode_block, inode_block_idx;

  inode_block = ext2_get_inode_block(ip, &inode_block_idx);

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
    if ((buf = buf_read(ip->block[0], ip->dev)) == NULL)
      panic("cannot read the data block");

    ip->rdev = *(uint16_t *) buf->data;

    buf_release(buf);
  }
}

void
ext2_write_inode(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2Inode *dp;
  unsigned inode_block, inode_block_idx;

  if (((ip->mode & EXT2_S_IFMASK) == EXT2_S_IFBLK) ||
      ((ip->mode & EXT2_S_IFMASK) == EXT2_S_IFCHR)) {
    if (ip->size == 0) {
      if (ext2_block_alloc(ip->dev, &ip->block[0], ip->ino) != 0)
        panic("Cannot allocate block");
      ip->size = sizeof(uint16_t);

      if ((buf = buf_read(ip->block[0], ip->dev)) == NULL)
        panic("cannot read the data block");

      *(uint16_t *) buf->data = ip->rdev;

      buf_release(buf);
    }
  }

  inode_block = ext2_get_inode_block(ip, &inode_block_idx);

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
ext2_delete_inode(struct Inode *ip)
{
  ext2_inode_trunc(ip);

  ip->mode = 0;
  ip->size = 0;
  ext2_write_inode(ip);

  ext2_inode_free(ip->dev, ip->ino);
}

/*
 * ----------------------------------------------------------------------------
 * Superblock operations
 * ----------------------------------------------------------------------------
 */

struct Ext2Superblock sb;

struct Inode *
ext2_mount(void)
{
  struct Buf *buf;

  if ((buf = buf_read(1, 0)) == NULL)
    panic("cannot read the superblock");

  memcpy(&sb, buf->data, sizeof(sb));
  buf_release(buf);

  if (sb.log_block_size != 0)
    panic("block size must be 1024 bytes");

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          sb.block_count * BLOCK_SIZE / (1024 * 1024),
          sb.inodes_count, sb.block_count);

  return fs_inode_get(2, 0);
}

ssize_t
ext2_readdir(struct Inode *dir, void *buf, FillDirFunc filldir, off_t off)
{
  struct Ext2DirEntry de;
  ssize_t nread;

  if (!S_ISDIR(dir->mode))
    panic("not a directory");

  if (off >= dir->size)
    return 0;

  if ((nread = ext2_dirent_read(dir, &de, off)) < 0)
    return nread;

  filldir(buf, de.name, de.name_len, off + de.rec_len, de.inode, de.file_type);

  return de.rec_len;
}
