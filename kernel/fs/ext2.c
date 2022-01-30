#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>

#include <kernel/fs/ext2.h>

/*
 * ----------------------------------------------------------------------------
 * Superblock operations
 * ----------------------------------------------------------------------------
 */

struct Ext2Superblock sb;

void
ext2_read_superblock(void)
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
}

/*
 * ----------------------------------------------------------------------------
 * Block operations
 * ----------------------------------------------------------------------------
 */



// Try to allocate a block from the block group descriptor pointed to by `gd`.
// If there is a free block, mark it as used and store its number into the
// memory location pointed to by `bstore`. Otherwise, return `-ENOMEM`.
static int
ext2_gd_block_alloc(struct Ext2GroupDesc *gd, uint32_t *bstore)
{
  uint32_t b, bi;

  if (gd->free_blocks_count == 0)
    return -ENOMEM;

  for (b = 0; b < sb.blocks_per_group; b += BITS_PER_BLOCK) {
    struct Buf *buf;
    uint32_t *map;

    if ((buf = buf_read(gd->block_bitmap + b, 0)) == NULL)
      panic("cannot read the bitmap block");

    map = (uint32_t *) buf->data;

    for (bi = 0; bi < BITS_PER_BLOCK; bi++) {
      if (map[bi / 32] & (1 << (bi % 32)))
        continue;

      map[bi / 32] |= (1 << (bi % 32));  
      gd->free_blocks_count--;

      buf_write(buf);
      buf_release(buf);

      *bstore = b + bi;

      return 0;
    }

    buf_release(buf);
  }

  // If free_blocks_count isn't zero, but we cannot find a free block, the
  // filesystem is corrupted.
  panic("cannot allocate block");
  return -ENOMEM;
}

int
ext2_block_alloc(uint32_t *bstore)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t block;

  // TODO: First try to allocate a new block in the same group as its inode
  
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  for (g = 0; g < sb.block_count / sb.blocks_per_group; g += gds_per_block) {
    if ((buf = buf_read(2 + (g / gds_per_block), 0)) == NULL)
      panic("cannot read the group descriptor table");

    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2GroupDesc *) buf->data + gi;
      if (ext2_gd_block_alloc(gd, &block) == 0) {
        buf_release(buf);
        *bstore = (g + gi) * sb.blocks_per_group + block;
        return 0;
      }
    }

    buf_release(buf);
  }
  
  return -ENOMEM;
}

/*
 * ----------------------------------------------------------------------------
 * Inode operations
 * ----------------------------------------------------------------------------
 */

// Try to allocate an inode from the block group descriptor pointed to by `gd`.
// If there is a free inode, mark it as used and store its number into the
// memory location pointed to by `istore`. Otherwise, return `-ENOMEM`.
static int
ext2_gd_inode_alloc(struct Ext2GroupDesc *gd, uint32_t *istore)
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
      if (ext2_gd_inode_alloc(gd, &inum) == 0) {
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

#define ADDRS_PER_BLOCK BLOCK_SIZE / sizeof(uint32_t)

unsigned
ext2_block_map(struct Inode *ip, unsigned block_no)
{
  struct Buf *buf;
  uint32_t addr, *ptr;
  unsigned bcnt;

  if (block_no < 12) {
    if ((addr = ip->block[block_no]) == 0) {
      if (ext2_block_alloc(&addr) != 0)
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
    if (ext2_block_alloc(&addr) != 0)
      panic("cannot allocate block");
    *ptr = addr;
  }

  while ((bcnt /= ADDRS_PER_BLOCK) > 0) {
    if ((buf = buf_read(addr, ip->dev)) == NULL)
      panic("cannot read the data block");

    ptr = (uint32_t *) buf->data;
    if ((addr = ptr[block_no / bcnt]) == 0) {
      if (ext2_block_alloc(&addr) != 0)
        panic("cannot allocate block");
      ptr[block_no / bcnt] = addr;
      buf_write(buf);
    }

    buf_release(buf);

    block_no %= bcnt;
  }

  return addr;
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

    if ((buf = buf_read(ext2_block_map(ip, 0), ip->dev)) == NULL)
      panic("cannot read the data block");
    dev = *(uint16_t *) buf->data;
    buf_release(buf);
  
    ip->major = (dev >> 8) & 0xFF;
    ip->minor = dev & 0xFF;
  }
}

ssize_t
ext2_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  size_t total, nread;
  uint8_t *dst;

  dst = (uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    if ((b = buf_read(ext2_block_map(ip, off / BLOCK_SIZE), ip->dev)) == NULL)
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

  src = (const uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    if ((b = buf_read(ext2_block_map(ip, off / BLOCK_SIZE), ip->dev)) == NULL)
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

#define DE_NAME_OFFSET    offsetof(struct Ext2DirEntry, name)

ssize_t
ext2_dir_read(struct Inode *dir, void *buf, size_t n, off_t *offp)
{
  struct Ext2DirEntry de;
  struct dirent *dp;
  ssize_t nread;
  off_t off;

  off = *offp;

  if (off >= dir->size)
    return 0;

  nread = fs_inode_read(dir, &de, DE_NAME_OFFSET, off);
  if (nread != DE_NAME_OFFSET)
    panic("Cannot read directory");

  if ((sizeof(struct dirent) + de.name_len) > n)
    return 0;

  dp = (struct dirent *) buf;
  dp->d_ino     = de.inode;
  dp->d_off     = off + de.rec_len;
  dp->d_reclen  = sizeof(struct dirent) + de.name_len;
  dp->d_namelen = de.name_len;
  dp->d_type    = de.file_type;

  nread = fs_inode_read(dir, dp->d_name, dp->d_namelen, off + nread);
  if (nread != de.name_len)
    panic("Cannot read directory");

  *offp = off + de.rec_len;

  return dp->d_reclen;
}

struct Inode *
ext2_dir_lookup(struct Inode *dir, const char *name)
{
  struct Ext2DirEntry de;
  off_t off;
  size_t name_len;
  ssize_t nread;

  name_len = strlen(name);

  for (off = 0; off < dir->size; off += de.rec_len) {
    nread = fs_inode_read(dir, &de, DE_NAME_OFFSET, off);
    if (nread != DE_NAME_OFFSET)
      panic("Cannot read directory");

    nread = fs_inode_read(dir, de.name, de.name_len, off + DE_NAME_OFFSET);
    if (nread != de.name_len)
      panic("Cannot read directory");

    if (de.name_len != name_len)
      continue;

    if (strncmp(de.name, name, de.name_len) == 0)
      return fs_inode_get(de.inode, 0);
  }

  return NULL;
}

int
ext2_dir_link(struct Inode *dir, char *name, unsigned inode, mode_t mode)
{
  struct Ext2DirEntry de, new_de;
  off_t off;
  ssize_t name_len, de_len, new_len;
  uint8_t file_type;

  name_len = strlen(name);
  if (name_len > NAME_MAX)
    return -ENAMETOOLONG;

  switch (mode & S_IFMT) {
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

  new_de.inode     = inode;
  new_de.name_len  = name_len;
  new_de.file_type = file_type;
  strncpy(new_de.name, name, ROUND_UP(name_len, sizeof(uint32_t)));

  for (off = 0; off < dir->size; off += de.rec_len) {
    if (ext2_inode_read(dir, &de, DE_NAME_OFFSET, off) != DE_NAME_OFFSET)
      panic("Cannot read directory");

    de_len = ROUND_UP(DE_NAME_OFFSET + de.name_len, sizeof(uint32_t));
    if ((de.rec_len - de_len) >= new_len) {
      // Found enough space
      new_de.rec_len = de.rec_len - de_len;
      de.rec_len = de_len;
      
      if (ext2_inode_write(dir, &de, DE_NAME_OFFSET, off) != DE_NAME_OFFSET)
        panic("Cannot write directory");
      if (ext2_inode_write(dir, &new_de, new_len, off + de_len) != new_len)
        panic("Cannot write directory");

      return 0;
    }
  }

  assert(off % BLOCK_SIZE == 0);

  new_de.rec_len = BLOCK_SIZE;
  if (ext2_inode_write(dir, &new_de, new_len, off) != new_len)
    panic("Cannot write directory");

  dir->size = off + BLOCK_SIZE;
  ext2_inode_update(dir);

  return 0;
}
