#include <kernel/core/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/fs/buf.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include "ext2.h"

/**
 * Fill the block with zeros.
 * 
 * @param block_id ID of the block.
 * @param dev      The device.
 * 
 * @retval 0 on success.
 */
int
ext2_block_zero(struct Ext2SuperblockData *sb, uint32_t block_id, uint32_t dev)
{
  struct Buf *buf;
  
  if ((buf = buf_read(block_id, sb->block_size, dev)) == NULL)
    k_panic("cannot read block %d", block_id);

  memset(buf->data, 0, sb->block_size);

  buf_write(buf);
  // TODO: recover from I/O errors!

  return 0;
}

// Try to allocate a block from the block group descriptor pointed to by `gd`.
// If there is a free block, mark it as used and store its number (relative to
// the group) into the memory location pointed to by `bstore`. Otherwise,
// return `-ENOMEM`.
static int
ext2_block_group_alloc(struct Ext2SuperblockData *sb, struct Ext2BlockGroup *gd, dev_t dev, uint32_t *bstore)
{
  if (gd->free_blocks_count == 0)
    return -ENOMEM;

  if (ext2_bitmap_alloc(sb, gd->block_bitmap, sb->blocks_per_group, dev,
                        bstore) < 0)
    // If free_blocks_count isn't zero, but we couldn't find a free block, the
    // filesystem is corrupted.
    k_panic("no free blocks");

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
ext2_block_alloc(struct Thread *thread, struct Ext2SuperblockData *sb, dev_t dev, uint32_t *bstore)
{
  int r;
  uid_t euid = (thread == NULL) ? 0 : thread->process->euid;
  
  uint32_t gd_start      = sb->block_size > 1024U ? 1 : 2;
  uint32_t gds_total     = sb->block_count / sb->blocks_per_group;
  uint32_t gds_per_block = sb->block_size / sizeof(struct Ext2BlockGroup);

  uint32_t g, gi;

  if ((r = k_mutex_lock(&sb->mutex)) < 0)
    return r;
  
  if (sb->free_blocks_count == 0) {
    k_mutex_unlock(&sb->mutex);
    return -ENOSPC;
  }

  if ((sb->free_blocks_count < sb->r_blocks_count) && (euid != 0)) {
    k_mutex_unlock(&sb->mutex);
    return -ENOSPC;
  }

  sb->free_blocks_count--;

  k_mutex_unlock(&sb->mutex);

  ext2_sb_sync(sb, dev);

  // TODO: do not exceed r_blocks_count for ordinary users!
  // TODO: update free_blocks_count

  // TODO: try to allocate a new block in the same group as its inode
  // TODO: try to allocate consecutive sequences of blocks

  // Scan all group descriptors for a free block
  for (g = 0; g < gds_total; g += gds_per_block) {
    struct Buf *buf;

    if ((buf = buf_read(gd_start + (g / gds_per_block), sb->block_size, dev)) == NULL)
      // TODO: recover from I/O errors
      k_panic("cannot read the group descriptor table");

    for (gi = 0; gi < MIN(gds_per_block, gds_total - g); gi++) {
      struct Ext2BlockGroup *gd = (struct Ext2BlockGroup *) buf->data + gi;
      uint32_t block_id;

      if (ext2_block_group_alloc(sb, gd, dev, &block_id) == 0) {
        buf_write(buf);
        // TODO: recover from I/O errors

        block_id += (g + gi) * sb->blocks_per_group;

        ext2_block_zero(sb, block_id, dev);

        *bstore = block_id;

        return 0;
      }
    }

    buf_release(buf);
  }

  if ((r = k_mutex_lock(&sb->mutex)) < 0)
    k_panic("TODO");

  sb->free_blocks_count++;
  k_mutex_unlock(&sb->mutex);

  ext2_sb_sync(sb, dev);

  k_warn("no free blocks");

  return -ENOMEM;
}

/**
 * Free a filesystem block.
 * 
 * @param dev The device the block belongs to.
 * @param bno The block number.
 */
void
ext2_block_free(struct Ext2SuperblockData *sb, dev_t dev, uint32_t bno)
{
  struct Buf *buf;
  struct Ext2BlockGroup *gd;
  uint32_t gds_per_block, gd_idx, g, gi;

  uint32_t gd_start = sb->block_size > 1024U ? 1 : 2;
  gds_per_block = sb->block_size / sizeof(struct Ext2BlockGroup);
  gd_idx = bno / sb->blocks_per_group;
  g  = gd_idx / gds_per_block;
  gi = gd_idx % gds_per_block;

  buf = buf_read(gd_start + g, sb->block_size, dev);

  gd = (struct Ext2BlockGroup *) buf->data + gi;

  ext2_bitmap_free(sb, gd->block_bitmap, dev, bno % sb->blocks_per_group);

  gd->free_blocks_count++;
  buf_write(buf);

  if (k_mutex_lock(&sb->mutex) < 0)
    k_panic("TODO");
  sb->free_blocks_count++;
  k_mutex_unlock(&sb->mutex);

  ext2_sb_sync(sb, dev);
}
