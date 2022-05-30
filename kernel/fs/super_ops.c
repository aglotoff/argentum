#include <assert.h>
#include <string.h>

#include <kernel/fs/buf.h>
#include <kernel/fs/ext2.h>
#include <kernel/fs/fs.h>

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
ext2_put_inode(struct Inode *ip)
{
  ext2_inode_trunc(ip);

  ip->mode = 0;
  ip->size = 0;
  ext2_write_inode(ip);

  ext2_inode_free(ip->dev, ip->ino);
}
