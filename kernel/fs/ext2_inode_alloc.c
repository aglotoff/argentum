#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/console.h>
#include <kernel/fs/buf.h>
#include <kernel/time.h>

#include "ext2.h"

// Try to allocate an inode from the inode group descriptor pointed to by `gd`.
// If there is a free inode, mark it as used and store its number into the
// memory location pointed to by `istore`. Otherwise, return `-ENOMEM`.
static int
ext2_inode_group_alloc(struct Ext2SuperblockData *sb, struct Ext2BlockGroup *gd, dev_t dev, uint32_t *istore)
{
  if (gd->free_inodes_count == 0)
    return -ENOMEM;

  if (ext2_bitmap_alloc(sb, gd->inode_bitmap, sb->inodes_per_group, dev, istore))
    // If free_inodes_count isn't zero, but we cannot find a free inode, the
    // filesystem is corrupted.
    panic("no free inodes");

  gd->free_inodes_count--;

  return 0;
}

static int
ext2_inode_init(struct Ext2SuperblockData *sb, dev_t dev, uint32_t table, uint32_t inum, uint16_t mode,
                dev_t rdev)
{
  unsigned inodes_per_block, itab_idx, inode_block_idx, inode_block;
  struct Ext2Inode *raw;
  struct Buf *buf;

  inodes_per_block = sb->block_size / sb->inode_size;
  itab_idx  = (inum - 1) % sb->inodes_per_group;
  inode_block      = table + itab_idx / inodes_per_block;
  inode_block_idx  = itab_idx % inodes_per_block;

  if ((buf = buf_read(inode_block, sb->block_size, dev)) == NULL)
    panic("cannot read the inode table");

  raw = (struct Ext2Inode *) &buf->data[sb->inode_size * inode_block_idx];
  memset(raw, 0, sb->inode_size);
  raw->mode  = mode;
  raw->ctime = raw->atime = raw->mtime = time_get_seconds();

  if (((mode & EXT2_S_IFMASK) == EXT2_S_IFBLK) ||
      ((mode & EXT2_S_IFMASK) == EXT2_S_IFCHR)) {
    ext2_block_alloc(sb, dev, &raw->block[0]);
    struct Buf *block_buf;

    block_buf = buf_read(raw->block[0], sb->block_size, dev);
    memmove(block_buf->data, &rdev, sizeof rdev);
    block_buf->flags |= BUF_DIRTY;
    buf_release(block_buf);

    raw->size = sizeof rdev;
  
    raw->blocks += sb->block_size / 512;
  }

  buf->flags |= BUF_DIRTY;
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
ext2_inode_alloc(struct Ext2SuperblockData *sb, mode_t mode, dev_t rdev, dev_t dev,
                 uint32_t *istore, uint32_t parent)
{
  struct Buf *buf;
  struct Ext2BlockGroup *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t inum;
  uint32_t gd_start      = sb->block_size > 1024U ? 1 : 2;

  gds_per_block = sb->block_size / sizeof(struct Ext2BlockGroup);

  // First try to find a free block in the same group as the specified inode
  g  = (parent - 1) / sb->inodes_per_group;
  gi = g % gds_per_block;
  g  = g - gi;

  if ((buf = buf_read(gd_start + (g / gds_per_block), sb->block_size, dev)) == NULL)
    panic("cannot read the group descriptor table");

  gd = (struct Ext2BlockGroup *) buf->data + gi;

  if (ext2_inode_group_alloc(sb, gd, dev, &inum) == 0) {
    uint32_t table;

    table = gd->inode_table;

    inum += 1 + (g + gi) * sb->inodes_per_group;

    buf->flags |= BUF_DIRTY;
    buf_release(buf);

    ext2_inode_init(sb, dev, table, inum, mode, rdev);

    if (istore)
      *istore = inum;

    return 0;
  }

  // Scan all group descriptors for a free inode
  for (g = 0; g < sb->inodes_count / sb->inodes_per_group; g += gds_per_block) {
    if ((buf = buf_read(gd_start + (g / gds_per_block), sb->block_size, dev)) == NULL)
      panic("cannot read the group descriptor table");

    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2BlockGroup *) buf->data + gi;

      if (ext2_inode_group_alloc(sb, gd, dev, &inum) == 0) {
        uint32_t table;

        table = gd->inode_table;

        buf->flags |= BUF_DIRTY;
        buf_release(buf);

        inum += 1 + (g + gi) * sb->inodes_per_group;

        ext2_inode_init(sb, dev, table, inum, mode, rdev);

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
ext2_inode_free(struct Ext2SuperblockData *sb, dev_t dev, uint32_t ino)
{
  struct Buf *buf;
  struct Ext2BlockGroup *gd;
  uint32_t gds_per_block, gd_idx, g, gi;
  uint32_t gd_start = sb->block_size > 1024U ? 1 : 2;

  gds_per_block = sb->block_size / sizeof(struct Ext2BlockGroup);
  gd_idx = (ino - 1) / sb->inodes_per_group;
  g  = gd_idx / gds_per_block;
  gi = gd_idx % gds_per_block;

  buf = buf_read(gd_start + g, sb->block_size, dev);

  gd = (struct Ext2BlockGroup *) buf->data + gi;

  ext2_bitmap_free(sb, gd->inode_bitmap, dev, (ino - 1) % sb->inodes_per_group);

  gd->free_inodes_count++;
  buf->flags |= BUF_DIRTY;

  buf_release(buf);
}
