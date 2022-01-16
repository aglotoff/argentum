#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "kernel.h"
#include "buf.h"
#include "console.h"
#include "ext2.h"
#include "fs.h"

static struct {
  struct Inode    buf[INODE_CACHE_SIZE];
  struct SpinLock lock;
  struct ListLink head;
} inode_cache;

static struct Inode *
fs_inode_get(unsigned inum)
{
  struct ListLink *l;
  struct Inode *ip, *empty;

  spin_lock(&inode_cache.lock);

  empty = NULL;
  LIST_FOREACH(&inode_cache.head, l) {
    ip = LIST_CONTAINER(l, struct Inode, cache_link);
    if (ip->num == inum) {
      ip->ref_count++;

      spin_unlock(&inode_cache.lock);

      return ip;
    }

    if (ip->ref_count == 0)
      empty = ip;
  }

  if (empty != NULL) {
    empty->ref_count = 1;
    empty->num = inum;
    empty->valid = 0;

    spin_unlock(&inode_cache.lock);

    return empty;
  }

  spin_unlock(&inode_cache.lock);

  return NULL;
}

struct Ext2Superblock sb;

static void
fs_read_superblock(void)
{
  struct Buf *buf;

  buf = buf_read(1);
  memcpy(&sb, buf->data, sizeof(sb));
  buf_release(buf);

  if (sb.log_block_size != 0)
    panic("block size must be 1024 bytes");

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          sb.block_count * BLOCK_SIZE / (1024 * 1024),
          sb.inodes_count, sb.block_count);
}

void
fs_inode_lock(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2BlockGroupDesc gd;
  unsigned gds_per_block, inodes_per_block;
  unsigned block_group, table_block, table_idx;
  unsigned inode_table_idx, inode_block_idx, inode_block;

  mutex_lock(&ip->mutex);

  if (ip->valid)
    return;

  // Determine which block group the inode belongs to
  block_group = (ip->num - 1) / sb.inodes_per_group;

  // Read the Block Group Descriptor corresponding to the Block Group which
  // contains the inode to be looked up
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2BlockGroupDesc);
  table_block = 2 + (block_group / gds_per_block);
  table_idx = (block_group % gds_per_block);

  buf = buf_read(table_block);
  memcpy(&gd, &buf->data[sizeof(gd) * table_idx], sizeof(gd));
  buf_release(buf);

  // From the Block Group Descriptor, extract the location of the block
  // group's inode table

  // Determine the index of the inode in the inode table. 
  inodes_per_block = BLOCK_SIZE / sb.inode_size;
  inode_table_idx = (ip->num - 1) % sb.inodes_per_group;
  inode_block = gd.inode_table + inode_table_idx / inodes_per_block;
  inode_block_idx = inode_table_idx % inodes_per_block;

  // Index the inode table (taking into account non-standard inode size)
  buf = buf_read(inode_block);
  memcpy(&ip->data, &buf->data[inode_block_idx * sb.inode_size],
         sizeof(ip->data));
  buf_release(buf);
}

void
fs_inode_put(struct Inode *ip)
{ 
  spin_lock(&inode_cache.lock);

  assert(ip->ref_count > 0);

  if (--ip->ref_count == 0) {
    list_remove(&ip->cache_link);
    list_add_front(&inode_cache.head, &ip->cache_link);
  }

  spin_unlock(&inode_cache.lock);
}

void
fs_inode_unlock(struct Inode *ip)
{
  if (!mutex_holding(&ip->mutex))
    panic("not holding buf");

  mutex_unlock(&ip->mutex);
}

static unsigned
fs_block_map(struct Inode *ip, unsigned block_no)
{
  struct Buf *buf;
  unsigned nindirect = BLOCK_SIZE / sizeof(uint32_t);
  unsigned bcnt, idx;

  if (block_no < 12)
    return ip->data.block[block_no];
  
  block_no -= 12;
  for (idx = 13, bcnt = nindirect; block_no >= bcnt; idx++, bcnt *= nindirect)
    ;
  idx = ip->data.block[idx];

  while (bcnt >= nindirect) {
    bcnt /= nindirect;

    buf = buf_read(idx);
    idx = ((uint32_t *) buf->data)[block_no / bcnt];
    buf_release(buf);

    block_no %= bcnt;
  }

  return idx;
}

ssize_t
fs_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  size_t total, nread;
  uint8_t *dst;

  // TODO: read device
  if (!mutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  if ((off < 0) || ((size_t) off > ip->data.size) || ((off + nbyte) < (size_t) off))
    return -1;

  if ((off + nbyte) > ip->data.size)
    nbyte = ip->data.size - off;
  
  dst = (uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    b = buf_read(fs_block_map(ip, off / BLOCK_SIZE));
    nread = MIN(BLOCK_SIZE - (size_t) off % BLOCK_SIZE, nbyte - total);
    memmove(dst, &((const uint8_t *) b->data)[off % BLOCK_SIZE], nread);
    buf_release(b);

    total += nread;
    dst   += nread;
  }

  return total;
}

void
fs_init(void)
{
  struct Inode *ip;
  struct Ext2DirEntry de;
  off_t off;
  
  spin_init(&inode_cache.lock, "inode_cache");
  list_init(&inode_cache.head);

  for (ip = inode_cache.buf; ip < &inode_cache.buf[INODE_CACHE_SIZE]; ip++) {
    mutex_init(&ip->mutex, "inode");
    list_init(&ip->wait_queue);

    list_add_back(&inode_cache.head, &ip->cache_link);
  }

  fs_read_superblock();

  // Read the contents of the root directory
  ip = fs_inode_get(2);
  fs_inode_lock(ip);

  for (off = 0; (size_t) off < ip->data.size; off += de.rec_len) {
    fs_inode_read(ip, &de, sizeof(de), off);
    fs_inode_read(ip, de.name, de.name_len,
                  off + offsetof(struct Ext2DirEntry, name));
    cprintf("%2d %.*s %d\n", de.inode, de.name_len, de.name, de.file_type);
  }

  fs_inode_unlock(ip);
  fs_inode_put(ip);
}
