#include <assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>

#include "ext2.h"

/**
 * @brief Get inode location on the disk
 * 
 * @param inode  Pointer to the inode to search for
 * @param offset Pointer to store the offset in bytes of this inode inside the
 *               containing filesystem block.
 *               
 * @return ID of the filesystem block containing this inode or 0 if there is
 *         an error.
 */
static uint32_t
ext2_inode_get_location(struct Inode *inode, uint32_t *offset)
{
  size_t block_size         = 1024U << ext2_sb.log_block_size;
  unsigned gds_per_block    = block_size / sizeof(struct Ext2BlockGroup);
  unsigned inodes_per_block = block_size / ext2_sb.inode_size;
  
  struct Buf *buf;
  struct Ext2BlockGroup *gd;
  unsigned block_group, gd_table_block, gd_table_idx;
  unsigned local_inode_idx, block;

  // 1. Determine which block group the inode belongs to and read the
  //    corresponding block group descriptor

  block_group    = (inode->ino - 1) / ext2_sb.inodes_per_group;
  gd_table_block = 2 + (block_group / gds_per_block);
  gd_table_idx   = (block_group % gds_per_block);

  if ((buf = buf_read(gd_table_block, inode->dev)) == NULL)
    return 0;

  gd = (struct Ext2BlockGroup *) buf->data + gd_table_idx;

  // 2. Determine the index in the local inode table of this group descriptor

  local_inode_idx  = (inode->ino - 1) % ext2_sb.inodes_per_group;

  block   = gd->inode_table + local_inode_idx / inodes_per_block;
  *offset = (local_inode_idx % inodes_per_block) * ext2_sb.inode_size;

  buf_release(buf);

  return block;
}

int
ext2_read_inode(struct Inode *inode)
{
  struct Buf *buf;
  struct Ext2Inode *raw;
  unsigned inode_block, inode_offset;

  inode_block = ext2_inode_get_location(inode, &inode_offset);

  // Index the inode table (taking into account non-standard inode size)
  if ((buf = buf_read(inode_block, inode->dev)) == NULL)
    return -EIO;

  raw = (struct Ext2Inode *) (buf->data + inode_offset);

  inode->mode        = raw->mode;
  inode->nlink       = raw->links_count;
  inode->uid         = raw->uid;
  inode->gid         = raw->gid;
  inode->size        = raw->size;
  inode->atime       = raw->atime;
  inode->mtime       = raw->mtime;
  inode->ctime       = raw->ctime;
  inode->ext2.blocks = raw->blocks;
  memmove(inode->ext2.block, raw->block, sizeof(inode->ext2.block));

  buf_release(buf);

  return 0;
}

int
ext2_write_inode(struct Inode *inode)
{
  struct Buf *buf;
  struct Ext2Inode *raw;
  unsigned block, offset;

  block = ext2_inode_get_location(inode, &offset);

  if ((buf = buf_read(block, inode->dev)) == NULL)
    return -EIO;

  raw = (struct Ext2Inode *) (buf->data + offset);

  // Update common fields
  raw->mode        = inode->mode;
  raw->links_count = inode->nlink;
  raw->uid         = inode->uid;
  raw->gid         = inode->gid;
  raw->size        = inode->size;
  raw->atime       = inode->atime;
  raw->mtime       = inode->mtime;
  raw->ctime       = inode->ctime;

  // Update ext2-specific fields
  raw->blocks      = inode->ext2.blocks;
  memmove(raw->block, inode->ext2.block, sizeof(inode->ext2.block));

  buf->flags |= BUF_DIRTY;

  buf_release(buf);

  return 0;
}

#define EXT2_MAX_DIRECT_BLOCKS  12

uint32_t
ext2_inode_get_block(struct Inode *inode, uint32_t n, int alloc)
{
  size_t blocks_inc = (1024U / 512U) << ext2_sb.log_block_size;
  uint32_t shift_per_lvl = (10 - 2 + ext2_sb.log_block_size);
  uint32_t lvl_limit, lvl_idx_mask, lvl_idx_shift;
  uint32_t id, *id_store;
  int lvl;

  if (n < EXT2_MAX_DIRECT_BLOCKS) {
    // Direct block
    id_store = &inode->ext2.block[n];

    if ((id = *id_store) == 0) {
      if (!alloc || (ext2_block_alloc(inode->dev, &id) != 0))
        return 0;

      *id_store = id;

      inode->ext2.blocks += blocks_inc;
      inode->flags |= FS_INODE_DIRTY;
    }

    return id;
  }

  n -= EXT2_MAX_DIRECT_BLOCKS;

  lvl_limit     = (1U << shift_per_lvl);
  lvl_idx_mask  = (1U << shift_per_lvl) - 1;
  lvl_idx_shift = 0;

  // Determine the level of indirect block the desired block belongs to
  for (lvl = 0; lvl < 3; lvl++) {
    if (n < lvl_limit)
      break;

    n -= lvl_limit;

    lvl_limit    <<= shift_per_lvl;
    lvl_idx_shift += shift_per_lvl;
  }

  // Block number too large
  if (lvl >= 3)
    return 0;

  // Get the ID of the first indirect block in the chain
  id_store = &inode->ext2.block[EXT2_MAX_DIRECT_BLOCKS + lvl];
  if ((id = *id_store) == 0) {
    if (!alloc || (ext2_block_alloc(inode->dev, &id) == 0))
      return 0;

    *id_store = id;
    inode->ext2.blocks += blocks_inc;
    inode->flags |= FS_INODE_DIRTY;
  }

  // Follow the chain of indirect blocks
  for ( ; lvl >= 0; lvl--) {
    struct Buf *buf;

    if ((buf = buf_read(id, inode->dev)) == NULL)
      // TODO: I/O error?
      return 0;
    
    id_store  = (uint32_t *) buf->data;
    id_store += (n >> lvl_idx_shift) & lvl_idx_mask;

    if ((id = *id_store) == 0) {
      if (!alloc || (ext2_block_alloc(inode->dev, &id) == 0)) {
        buf_release(buf);
        return 0;
      }

      *id_store = id;
      inode->ext2.blocks += blocks_inc;
      inode->flags |= FS_INODE_DIRTY;

      buf->flags |= BUF_DIRTY;
    }
    buf_release(buf);
  
    lvl_idx_shift -= shift_per_lvl;
  }

  return id;
}

#define INDIRECT_BLOCKS   BLOCK_SIZE / sizeof(uint32_t)

void
ext2_inode_trunc(struct Inode *inode)
{
  struct Buf *buf;
  uint32_t *a, i;

  for (i = 0; i < EXT2_MAX_DIRECT_BLOCKS; i++) {
    if (inode->ext2.block[i] == 0) {
      assert(inode->ext2.blocks == 0);
      return;
    }

    ext2_block_free(inode->dev, inode->ext2.block[i]);
    inode->ext2.block[i] = 0;
    inode->ext2.blocks--;
  }

  if (inode->ext2.block[EXT2_MAX_DIRECT_BLOCKS] == 0)
    return;
  
  buf = buf_read(inode->ext2.block[EXT2_MAX_DIRECT_BLOCKS], inode->dev);
  a = (uint32_t *) buf->data;

  for (i = 0; i < INDIRECT_BLOCKS; i++) {
    if (a[i] == 0)
      break;
    
    ext2_block_free(inode->dev, a[i]);
    a[i] = 0;
    inode->ext2.blocks--;
  }

  buf->flags |= BUF_DIRTY;
  buf_release(buf);

  ext2_block_free(inode->dev, inode->ext2.block[EXT2_MAX_DIRECT_BLOCKS]);
  inode->ext2.block[EXT2_MAX_DIRECT_BLOCKS] = 0;
  inode->ext2.blocks--;
  
  assert(inode->ext2.blocks == 0);
}

ssize_t
ext2_inode_read(struct Inode *inode, void *buf, size_t nbyte, off_t off)
{
  size_t block_size = (1024U << ext2_sb.log_block_size);
  size_t total, n;
  uint8_t *dst;

  dst = (uint8_t *) buf;
  for (total = 0; total < nbyte; total += n, off += n, dst += n) {
    uint32_t block_id = ext2_inode_get_block(inode, off / block_size, 0);
  
    n = MIN(nbyte - total, block_size - off % block_size);
  
    if (block_id == 0) {
      // Zero block in a sparse file - fill with zeros
      // TODO: could be an error
      memset(dst, 0, n);
    } else {
      // Read the block contents
      struct Buf *buf;

      if ((buf = buf_read(block_id, inode->dev)) == NULL) {
        buf_release(buf);
        return -EIO;
      }

      memmove(dst, &buf->data[off % block_size], n);

      buf_release(buf);
    }
  }

  return total;
}

ssize_t
ext2_inode_write(struct Inode *inode, const void *buf, size_t nbyte, off_t off)
{
  size_t block_size = (1024U << ext2_sb.log_block_size);
  size_t total, n;
  const uint8_t *src;

  src = (const uint8_t *) buf;
  for (total = 0; total < nbyte; total += n, off += n, src += n) {
    uint32_t block_id = ext2_inode_get_block(inode, off / block_size, 1);
    struct Buf *buf;

    n = MIN(nbyte - total, block_size - off % block_size);
  
    if (block_id == 0)
      return -ENOMEM;

    if ((buf = buf_read(block_id, inode->dev)) == NULL) {
      buf_release(buf);
      return -EIO;
    }

    memmove(&buf->data[off % block_size], src, n);
    buf->flags |= BUF_DIRTY;

    buf_release(buf);
  }

  return total;
}
