#include <assert.h>
#include <errno.h>
#include <string.h>

#include <drivers/rtc.h>
#include <fs/buf.h>
#include <fs/ext2.h>
#include <fs/fs.h>

// Try to allocate an inode from the block group descriptor pointed to by `gd`.
// If there is a free inode, mark it as used and store its number into the
// memory location pointed to by `istore`. Otherwise, return `-ENOMEM`.
static int
ext2_gd_inode_alloc(struct Ext2GroupDesc *gd, dev_t dev, uint32_t *istore)
{
  if (gd->free_inodes_count == 0)
    return -ENOMEM;

  if ((ext2_bitmap_alloc(gd->inode_bitmap, sb.inodes_per_group, dev, istore)))
    // If free_inodes_count isn't zero, but we cannot find a free inode, the
    // filesystem is corrupted.
    panic("no free inodes");

  gd->free_inodes_count--;

  return 0;
}

static int
ext2_inode_new(uint32_t table, uint32_t inum, uint16_t mode)
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
  dp->ctime = dp->atime = dp->mtime = rtc_time();

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

  if (ext2_gd_inode_alloc(gd, dev, &inum) == 0) {
    uint32_t table;

    table = gd->inode_table;

    buf_write(buf);
    buf_release(buf);

    inum += 1 + (g + gi) * sb.inodes_per_group;

    ext2_inode_new(table, inum, mode);

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
        uint32_t table;

        table = gd->inode_table;

        buf_write(buf);
        buf_release(buf);

        inum += 1 + (g + gi) * sb.inodes_per_group;

        ext2_inode_new(table, inum, mode);

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
