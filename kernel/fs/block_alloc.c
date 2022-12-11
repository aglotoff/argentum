#include <assert.h>
#include <errno.h>
#include <string.h>

#include <argentum/fs/buf.h>
#include <argentum/fs/ext2.h>
#include <argentum/fs/fs.h>

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
ext2_gd_block_alloc(struct Ext2GroupDesc *gd, dev_t dev, uint32_t *bstore)
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
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t block_no;

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  // First try to find a free block in the same group as the specified inode
  g  = (ino - 1) / sb.inodes_per_group;
  gi = g % gds_per_block;
  g  = g - gi;

  if ((buf = buf_read(GD_BLOCKS_BASE + (g / gds_per_block), dev)) == NULL)
    panic("cannot read the group descriptor table");

  gd = (struct Ext2GroupDesc *) buf->data + gi;

  if (ext2_gd_block_alloc(gd, dev, &block_no) == 0) {
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
      gd = (struct Ext2GroupDesc *) buf->data + gi;

      if (ext2_gd_block_alloc(gd, dev, &block_no) == 0) {
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
