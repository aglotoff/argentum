#include <assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/fs/buf.h>
#include <kernel/fs/ext2.h>
#include <kernel/fs/fs.h>

/*
 * ----------------------------------------------------------------------------
 * Allocating Blocks
 * ----------------------------------------------------------------------------
 */

// Block descriptors begin at block 2
#define GD_BLOCKS_BASE  2

// Try to allocate a block from the block group descriptor pointed to by `gd`.
// If there is a free block, mark it as used and store its number (relative to
// the group) into the memory location pointed to by `bstore`. Otherwise,
// return `-ENOMEM`.
static int
fs_gd_block_alloc(struct Ext2GroupDesc *gd, dev_t dev, uint32_t *bstore)
{
  uint32_t b, bi;

  if (gd->free_blocks_count == 0)
    return -ENOMEM;

  for (b = 0; b < sb.blocks_per_group; b += BITS_PER_BLOCK) {
    struct Buf *buf;
    uint32_t *bmap;

    if ((buf = buf_read(gd->block_bitmap + b, dev)) == NULL)
      panic("cannot read the bitmap block");

    bmap = (uint32_t *) buf->data;

    for (bi = 0; bi < BITS_PER_BLOCK; bi++) {
      if (bmap[bi / 32] & (1 << (bi % 32)))
        continue;

      bmap[bi / 32] |= (1 << (bi % 32));  
      gd->free_blocks_count--;

      buf_write(buf);
      buf_release(buf);

      *bstore = b + bi;

      return 0;
    }

    buf_release(buf);
  }

  // If free_blocks_count isn't zero, but we couldn't find a free block, the
  // filesystem is corrupted.
  panic("no free blocks");
  return -ENOMEM;
}

/**
 * Allocate a zeroed block.
 * 
 * @param dev    The device to allocate block from.
 * @param bstore Pointer to the memory location where to store the allocated
 *               block number.
 *
 * @retval 0       Success
 * @retval -ENOMEM Couldn't find a free block.
 */
int
fs_block_alloc(dev_t dev, uint32_t *bstore)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t block;

  // TODO: First try to allocate a new block in the same group as its inode
  
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  for (g = 0; g < sb.block_count / sb.blocks_per_group; g += gds_per_block) {
    if ((buf = buf_read(GD_BLOCKS_BASE + (g / gds_per_block), dev)) == NULL)
      panic("cannot read the group descriptor table");

    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2GroupDesc *) buf->data + gi;
      if (fs_gd_block_alloc(gd, dev, &block) == 0) {
        buf_release(buf);

        block += (g + gi) * sb.blocks_per_group;

        buf = buf_read(block, dev);
        memset(buf->data, 0, BLOCK_SIZE);
        buf_release(buf);

        *bstore = block;

        return 0;
      }
    }

    buf_release(buf);
  }
  
  return -ENOMEM;
}

/*
 * ----------------------------------------------------------------------------
 * Freeing blocks
 * ----------------------------------------------------------------------------
 */

// Free the block number `block` from the block group descriptor pointed to by
// `gd`.
static void
fs_gd_block_free(struct Ext2GroupDesc *gd, dev_t dev, uint32_t block)
{
  uint32_t b, bi;
  struct Buf *buf;
  uint32_t *map;

  b  = block / sb.blocks_per_group;
  bi = block % sb.blocks_per_group;

  if ((buf = buf_read(gd->block_bitmap + b, dev)) == NULL)
    panic("cannot read the bitmap block");

  map = (uint32_t *) buf->data;

  if (!(map[bi / 32] & (1 << (bi % 32))))
    panic("not allocated");

  map[bi / 32] &= ~(1 << (bi % 32));
    
  gd->free_blocks_count++;
    
  buf_write(buf);
  buf_release(buf);
}

/**
 * Free a disk block.
 * 
 * @param dev The device the block belongs to.
 * @param bno The block number.
 */
void
fs_block_free(dev_t dev, uint32_t bno)
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
  fs_gd_block_free(gd, dev, bno % sb.blocks_per_group);

  buf_release(buf);
}
