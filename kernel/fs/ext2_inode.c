#include <kernel/core/assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/console.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>

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
ext2_locate_inode(struct Inode *inode, uint32_t *offset)
{
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);
  unsigned gds_per_block    = sb->block_size / sizeof(struct Ext2BlockGroup);
  unsigned inodes_per_block = sb->block_size / sb->inode_size;

  struct Buf *buf;
  struct Ext2BlockGroup *gd;
  unsigned block_group, gd_table_block, gd_table_idx;
  unsigned local_inode_idx, block;
  uint32_t gd_start      = sb->block_size > 1024U ? 1 : 2;

  // 1. Determine which block group the inode belongs to and read the
  //    corresponding block group descriptor

  block_group    = (inode->ino - 1) / sb->inodes_per_group;
  gd_table_block = gd_start + (block_group / gds_per_block);
  gd_table_idx   = (block_group % gds_per_block);

  if ((buf = buf_read(gd_table_block, sb->block_size, inode->dev)) == NULL)
    return 0;

  gd = (struct Ext2BlockGroup *) buf->data + gd_table_idx;

  // 2. Determine the index in the local inode table of this group descriptor

  local_inode_idx  = (inode->ino - 1) % sb->inodes_per_group;

  block   = gd->inode_table + local_inode_idx / inodes_per_block;
  *offset = (local_inode_idx % inodes_per_block) * sb->inode_size;

  buf_release(buf);

  return block;
}

int
ext2_inode_read(struct Process *process, struct Inode *inode)
{
  struct Buf *buf;
  struct Ext2Inode *raw;
  struct Ext2InodeExtra *extra;
  unsigned long inode_block, inode_offset;
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);

  inode_block = ext2_locate_inode(inode, &inode_offset);

  if ((buf = buf_read(inode_block, sb->block_size, inode->dev)) == NULL)
    return -EIO;

  raw = (struct Ext2Inode *) (buf->data + inode_offset);

  // Read common fields
  inode->mode  = raw->mode;
  inode->nlink = raw->links_count;
  inode->uid   = raw->uid;
  inode->gid   = raw->gid;
  inode->size  = raw->size;
  inode->atime = raw->atime;
  inode->mtime = raw->mtime;
  inode->ctime = raw->ctime;

  // cprintf("%p\n", raw->flags);

  extra = (struct Ext2InodeExtra *) inode->extra;

  // Read ext2-specific fields
  extra->blocks = raw->blocks;
  memmove(extra->block, raw->block, sizeof(extra->block));

  if (S_ISCHR(inode->mode) || S_ISBLK(inode->mode)) {
    ext2_read_data(process, inode, &inode->rdev, sizeof(inode->rdev), 0);
  }

  buf_release(buf);

  return 0;
}

int
ext2_inode_write(struct Process *, struct Inode *inode)
{
  struct Buf *buf;
  struct Ext2Inode *raw;
  unsigned long block, offset;
  struct Ext2InodeExtra *extra;
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);

  block = ext2_locate_inode(inode, &offset);

  if ((buf = buf_read(block, sb->block_size, inode->dev)) == NULL)
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

  extra = (struct Ext2InodeExtra *) inode->extra;

  // Update ext2-specific fields
  raw->blocks = extra->blocks;
  memmove(raw->block, extra->block, sizeof(extra->block));

  buf_write(buf);

  return 0;
}

#define EXT2_MAX_DIRECT_BLOCKS  12

uint32_t
ext2_inode_get_block(struct Inode *inode, uint32_t n, int alloc)
{
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);
  size_t blocks_inc = (1024U / 512U) << sb->log_block_size;
  uint32_t shift_per_lvl = (10 - 2 + sb->log_block_size);
  uint32_t lvl_limit, lvl_idx_mask, lvl_idx_shift;
  uint32_t id, *id_store;
  struct Ext2InodeExtra *extra = (struct Ext2InodeExtra *) inode->extra;
  int lvl;

  if (n < EXT2_MAX_DIRECT_BLOCKS) {
    // Direct block
    id_store = &extra->block[n];

    if ((id = *id_store) == 0) {
      // TODO: why process_current?
      if (!alloc || (ext2_block_alloc(process_current(), sb, inode->dev, &id) != 0))
        return 0;

      *id_store = id;

      extra->blocks += blocks_inc;
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
  id_store = &extra->block[EXT2_MAX_DIRECT_BLOCKS + lvl];
  if ((id = *id_store) == 0) {
    // TODO: why process_current()?
    if (!alloc || (ext2_block_alloc(process_current(), sb, inode->dev, &id) != 0))
      return 0;

    *id_store = id;
    extra->blocks += blocks_inc;
    inode->flags |= FS_INODE_DIRTY;
  }

  // Follow the chain of indirect blocks
  for ( ; lvl >= 0; lvl--) {
    struct Buf *buf;

    if ((buf = buf_read(id, sb->block_size, inode->dev)) == NULL)
      // TODO: I/O error?
      return 0;
    
    id_store  = (uint32_t *) buf->data;
    id_store += (n >> lvl_idx_shift) & lvl_idx_mask;

    if ((id = *id_store) == 0) {
      // TODO: why process_current()?
      if (!alloc || (ext2_block_alloc(process_current(), sb, inode->dev, &id) != 0)) {
        buf_release(buf);
        return 0;
      }

      *id_store = id;
      extra->blocks += blocks_inc;
      inode->flags |= FS_INODE_DIRTY;

      buf_write(buf);
    } else {
      buf_release(buf);
    }
    
    lvl_idx_shift -= shift_per_lvl;
  }

  return id;
}

static void
ext2_trunc_indirect(struct Inode *inode, uint32_t *id_store, int lvl, size_t to)
{
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);
  size_t blocks_inc    = (1024U / 512U) << sb->log_block_size;
  size_t shift_per_lvl = (10 - 2 + sb->log_block_size);
  size_t inc           = 1U << (shift_per_lvl * lvl);
  struct Ext2InodeExtra *extra = (struct Ext2InodeExtra *) inode->extra;

  uint32_t id = *id_store;

  if (id == 0)
    return;

  if (lvl >= 0) {
    struct Buf *buf = buf_read(id, sb->block_size, inode->dev);
    uint32_t *ids = (uint32_t *) buf->data;
    size_t i;

    for (i = to; i < (inc << shift_per_lvl); i = ROUND_DOWN(i + inc, inc))
      ext2_trunc_indirect(inode, &ids[i / inc], lvl - 1, i % inc);

    buf_write(buf);
  }

  if (to == 0) {
    ext2_block_free(sb, inode->dev, id);
    extra->blocks -= blocks_inc;
    *id_store = 0;
  }
}

void
ext2_trunc(struct Process *, struct Inode *inode, off_t length)
{
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);
  size_t blocks_inc = (1024U / 512U) << sb->log_block_size;
  size_t n          = (length + sb->block_size - 1) / sb->block_size;
  size_t end        = (inode->size + sb->block_size - 1) / sb->block_size;
  struct Ext2InodeExtra *extra = (struct Ext2InodeExtra *) inode->extra;
  
  uint32_t shift_per_lvl = (10 - 2 + sb->log_block_size);
  uint32_t lvl_start, lvl_limit;

  // FIXME: symlinks

  int lvl;

  // Free direct blocks
  for ( ; (n < end) && (n < EXT2_MAX_DIRECT_BLOCKS); n++) {
    if (extra->block[n] != 0) {
      ext2_block_free(sb, inode->dev, extra->block[n]);
      extra->block[n] = 0;
      extra->blocks -= blocks_inc;
    }
  }

  lvl_start = EXT2_MAX_DIRECT_BLOCKS;
  lvl_limit = (1U << shift_per_lvl);

  for (lvl = 0; (lvl < 3) && (n < end); lvl++) {
    size_t lvl_end = lvl_start + lvl_limit;

    if (n <= lvl_end) {
      ext2_trunc_indirect(inode,
                          &extra->block[EXT2_MAX_DIRECT_BLOCKS + lvl], lvl,
                          n - lvl_start);
    }

    n = lvl_end;

    lvl_start  += lvl_limit;
    lvl_limit <<= shift_per_lvl;
  }
}

ssize_t
ext2_read_data(struct Process *, struct Inode *inode, void *p, size_t nbyte, off_t off)
{
  size_t total, n;
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);
  char *va = (char *) p;

  for (total = 0; total < nbyte; total += n, off += n, va += n) {
    uint32_t block_id = ext2_inode_get_block(inode, off / sb->block_size, 0);

    n = MIN(nbyte - total, sb->block_size - off % sb->block_size);
  
    if (block_id == 0) {
      // Zero block in a sparse file - fill with zeros
      // TODO: could be an error
      k_panic("zero block");
    } else {
      // Read the block contents
      struct Buf *buf;

      if ((buf = buf_read(block_id, sb->block_size, inode->dev)) == NULL) {
        return -EIO;
      }

      memmove((void *) va, &buf->data[off % sb->block_size], n);

      buf_release(buf);
    }
  }

  return total;
}

ssize_t
ext2_read(struct IpcRequest *req, struct Inode *inode, size_t nbyte, off_t off)
{
  size_t total, n;
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);

  for (total = 0; total < nbyte; total += n, off += n) {
    uint32_t block_id = ext2_inode_get_block(inode, off / sb->block_size, 0);
    int r;
  
    n = MIN(nbyte - total, sb->block_size - off % sb->block_size);
  
    if (block_id == 0) {
      // Zero block in a sparse file - fill with zeros
      // TODO: could be an error
      k_panic("zero block");
    } else {
      // Read the block contents
      struct Buf *buf;

      if ((buf = buf_read(block_id, sb->block_size, inode->dev)) == NULL) {
        return -EIO;
      }

      if ((r = ipc_request_write(req, &buf->data[off % sb->block_size], n)) < 0) {
        buf_release(buf);
        return r;
      }

      buf_release(buf);
    }
  }

  return total;
}

ssize_t
ext2_write(struct Process *process, struct Inode *inode, uintptr_t va, size_t nbyte, off_t off)
{
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (inode->fs->extra);
  size_t total, n;

  for (total = 0; total < nbyte; total += n, off += n, va += n) {
    uint32_t block_id = ext2_inode_get_block(inode, off / sb->block_size, 1);
    struct Buf *buf;

    n = MIN(nbyte - total, sb->block_size - off % sb->block_size);
  
    if (block_id == 0)
      return -ENOMEM;

    if ((buf = buf_read(block_id, sb->block_size, inode->dev)) == NULL) {
      return -EIO;
    }

    vm_space_copy_in(process, &buf->data[off % sb->block_size], va, n);
  
    buf_write(buf);
  }

  return total;
}
