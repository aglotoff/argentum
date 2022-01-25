#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include "kernel.h"
#include "kobject.h"
#include "buf.h"
#include "console.h"
#include "ext2.h"
#include "fs.h"

/*
 * ----------------------------------------------------------------------------
 * Superblock operations
 * ----------------------------------------------------------------------------
 */

static struct Ext2Superblock sb;

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

/*
 * ----------------------------------------------------------------------------
 * Block operations
 * ----------------------------------------------------------------------------
 */

// The number of bits per bitmap block
#define BITS_PER_BLOCK   (BLOCK_SIZE * 8)

// Try to allocate a block from the block group descriptor pointed to by `gd`.
// If there is a free block, mark it as used and store its number into the
// memory location pointed to by `bstore`. Otherwise, return `-ENOMEM`.
static int
fs_gd_block_alloc(struct Ext2GroupDesc *gd, uint32_t *bstore)
{
  uint32_t b, bi;

  if (gd->free_blocks_count == 0)
    return -ENOMEM;

  for (b = 0; b < sb.blocks_per_group; b += BITS_PER_BLOCK) {
    struct Buf *buf;
    uint32_t *map;

    buf = buf_read(gd->block_bitmap + b);
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
fs_block_alloc(uint32_t *bstore)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  uint32_t gds_per_block, g, gi;
  uint32_t block;

  // TODO: First try to allocate a new block in the same group as its inode
  
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  for (g = 0; g < sb.block_count / sb.blocks_per_group; g += gds_per_block) {
    buf = buf_read(2 + (g / gds_per_block));
    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2GroupDesc *) buf->data + gi;
      if (fs_gd_block_alloc(gd, &block) == 0) {
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

static struct {
  struct Inode    buf[INODE_CACHE_SIZE];
  struct SpinLock lock;
  struct ListLink head;
} inode_cache;

struct Inode *
fs_inode_get(ino_t ino, dev_t dev)
{
  struct ListLink *l;
  struct Inode *ip, *empty;

  spin_lock(&inode_cache.lock);

  empty = NULL;
  LIST_FOREACH(&inode_cache.head, l) {
    ip = LIST_CONTAINER(l, struct Inode, cache_link);
    if ((ip->ino == ino) && (ip->dev == dev)) {
      ip->ref_count++;
      spin_unlock(&inode_cache.lock);

      return ip;
    }

    if (ip->ref_count == 0)
      empty = ip;
  }

  if (empty != NULL) {
    empty->ref_count = 1;
    empty->ino       = ino;
    empty->dev       = dev;
    empty->valid     = 0;

    spin_unlock(&inode_cache.lock);

    return empty;
  }

  spin_unlock(&inode_cache.lock);

  return NULL;
}

// Try to allocate an inode from the block group descriptor pointed to by `gd`.
// If there is a free inode, mark it as used and store its number into the
// memory location pointed to by `istore`. Otherwise, return `-ENOMEM`.
static int
fs_gd_inode_alloc(struct Ext2GroupDesc *gd, uint32_t *istore)
{
  uint32_t b, bi;

  if (gd->free_inodes_count == 0)
    return -ENOMEM;

  for (b = 0; b < sb.blocks_per_group; b += BITS_PER_BLOCK) {
    struct Buf *buf;
    uint32_t *map;

    buf = buf_read(gd->inode_bitmap + b);
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

struct Inode *
fs_inode_alloc(mode_t mode, dev_t dev)
{
  struct Buf *buf;
  struct Ext2GroupDesc *gd;
  struct Inode *inode;
  uint32_t gds_per_block, g, gi;
  uint32_t inum;

  // TODO: First try to allocate a new inode in the same group as its parent

  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);

  for (g = 0; g < sb.inodes_count / sb.inodes_per_group; g += gds_per_block) {
    buf = buf_read(2 + (g / gds_per_block));
    for (gi = 0; gi < gds_per_block; gi++) {
      gd = (struct Ext2GroupDesc *) buf->data + gi;
      if (fs_gd_inode_alloc(gd, &inum) == 0) {
        unsigned inodes_per_block, itab_idx, inode_block_idx, inode_block;
        struct Ext2Inode *dp;

        inum += 1 + (g + gi) * sb.inodes_per_group;

        inodes_per_block = BLOCK_SIZE / sb.inode_size;
        itab_idx  = (inum - 1) % sb.inodes_per_group;
        inode_block      = gd->inode_table + itab_idx / inodes_per_block;
        inode_block_idx  = itab_idx % inodes_per_block;

        buf_release(buf);

        inode = fs_inode_get(inum, dev);
        if (inode == NULL)
          panic("cannot get inode (%u)", inum);

        inode->mode = mode;

        buf = buf_read(inode_block);

        dp = (struct Ext2Inode *) &buf->data[sb.inode_size * inode_block_idx];
        memset(dp, 0, sb.inode_size);
        dp->mode = mode;

        buf_write(buf);
        buf_release(buf);

        return inode;
      }
    }

    buf_release(buf);
  }
  
  return NULL;
}

void
fs_inode_update(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2GroupDesc gd;
  struct Ext2Inode *dp;
  unsigned gds_per_block, inodes_per_block;
  unsigned block_group, table_block, table_idx;
  unsigned inode_table_idx, inode_block_idx, inode_block;
  
  if (!mutex_holding(&ip->mutex))
    panic("caller must hold ip");

  // Determine which block group the inode belongs to
  block_group = (ip->ino - 1) / sb.inodes_per_group;

  // Read the Block Group Descriptor corresponding to the Block Group which
  // contains the inode to be looked up
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  table_block = 2 + (block_group / gds_per_block);
  table_idx = (block_group % gds_per_block);

  buf = buf_read(table_block);
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
  buf = buf_read(inode_block);

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
fs_inode_lock(struct Inode *ip)
{
  struct Buf *buf;
  struct Ext2GroupDesc gd;
  struct Ext2Inode *dp;
  unsigned gds_per_block, inodes_per_block;
  unsigned block_group, table_block, table_idx;
  unsigned inode_table_idx, inode_block_idx, inode_block;

  mutex_lock(&ip->mutex);

  if (ip->valid)
    return;

  // Determine which block group the inode belongs to
  block_group = (ip->ino - 1) / sb.inodes_per_group;

  // Read the Block Group Descriptor corresponding to the Block Group which
  // contains the inode to be looked up
  gds_per_block = BLOCK_SIZE / sizeof(struct Ext2GroupDesc);
  table_block = 2 + (block_group / gds_per_block);
  table_idx = (block_group % gds_per_block);

  buf = buf_read(table_block);
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
  buf = buf_read(inode_block);

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

  if (!ip->mode)
    panic("no mode");

  ip->valid = 1;
}

struct Inode *
fs_inode_dup(struct Inode *ip)
{
  spin_lock(&inode_cache.lock);
  ip->ref_count++;
  spin_unlock(&inode_cache.lock);

  return ip;
}

void
fs_inode_unlock(struct Inode *ip)
{  
  if (!mutex_holding(&ip->mutex))
    panic("not holding buf");

  mutex_unlock(&ip->mutex);
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

#define ADDRS_PER_BLOCK BLOCK_SIZE / sizeof(uint32_t)

static unsigned
fs_block_map(struct Inode *ip, unsigned block_no)
{
  struct Buf *buf;
  uint32_t addr, *ptr;
  unsigned bcnt;

  if (block_no < 12) {
    if ((addr = ip->block[block_no]) == 0) {
      if (fs_block_alloc(&addr) != 0)
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
    if (fs_block_alloc(&addr) != 0)
      panic("cannot allocate block");
    *ptr = addr;
  }

  while ((bcnt /= ADDRS_PER_BLOCK) > 0) {
    buf = buf_read(addr);

    ptr = (uint32_t *) buf->data;
    if ((addr = ptr[block_no / bcnt]) == 0) {
      if (fs_block_alloc(&addr) != 0)
        panic("cannot allocate block");
      ptr[block_no / bcnt] = addr;
      buf_write(buf);
    }

    buf_release(buf);

    block_no %= bcnt;
  }

  return addr;
}

ssize_t
fs_inode_read(struct Inode *ip, void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  size_t total, nread;
  uint8_t *dst;

  if (!mutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  // TODO: read device

  if ((off > ip->size) || ((off + nbyte) < off))
    return -1;

  if ((off + nbyte) > ip->size)
    nbyte = ip->size - off;
  
  dst = (uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    b = buf_read(fs_block_map(ip, off / BLOCK_SIZE));

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
fs_inode_write(struct Inode *ip, const void *buf, size_t nbyte, off_t off)
{
  struct Buf *b;
  size_t total, nwrite;
  const uint8_t *src;

  if (!mutex_holding(&ip->mutex))
    panic("not holding ip->mutex");

  // TODO: write device

  if ((off + nbyte) < off)
    return -1;
  
  src = (const uint8_t *) buf;
  total = 0;
  while (total < nbyte) {
    b = buf_read(fs_block_map(ip, off / BLOCK_SIZE));

    nwrite = MIN(BLOCK_SIZE - (size_t) off % BLOCK_SIZE, nbyte - total);
    memmove(&((uint8_t *) b->data)[off % BLOCK_SIZE], src, nwrite);

    buf_write(b);
    buf_release(b);

    total += nwrite;
    src   += nwrite;
    off   += nwrite;
  }

  if ((total > 0) && ((size_t) off > ip->size)) {
    ip->size = off;
    fs_inode_update(ip);
  }

  return total;
}

ssize_t
fs_inode_getdents(struct Inode *dir, void *buf, size_t n, off_t *off)
{
  struct Ext2DirEntry de;
  struct dirent *dp;
  size_t total;
  char *dst;

  if ((dir->mode & EXT2_S_IFMASK) != EXT2_S_IFDIR)
    return -ENOTDIR;
  
  dst = (char *) buf;
  total = 0;
  while (*off < dir->size) {
    ssize_t nread;
    
    nread = fs_inode_read(dir, &de, offsetof(struct Ext2DirEntry, name), *off);
    if (nread != offsetof(struct Ext2DirEntry, name))
      return -EINVAL;
    
    if ((sizeof(struct dirent) + de.name_len) > n)
      break;

    dp = (struct dirent *) dst;
    dp->d_ino     = de.inode;
    dp->d_off     = *off + de.rec_len;
    dp->d_reclen  = sizeof(struct dirent) + de.name_len;
    dp->d_namelen = de.name_len;
    dp->d_type    = de.file_type;

    nread = fs_inode_read(dir, dp->d_name, dp->d_namelen, *off + nread);
    if (nread != de.name_len)
      return -EINVAL;

    *off += de.rec_len;

    total += dp->d_reclen;
    dst   += dp->d_reclen;
  }

  return total;
}

struct Inode *
fs_dir_lookup(struct Inode *dir, const char *name)
{
  struct Ext2DirEntry de;
  off_t off;

  if ((dir->mode & EXT2_S_IFMASK) != EXT2_S_IFDIR)
    panic("not a directory");

  for (off = 0; (size_t) off < dir->size; off += de.rec_len) {
    fs_inode_read(dir, &de, offsetof(struct Ext2DirEntry, name), off);

    if (de.name_len == strlen(name)) {
      fs_inode_read(dir, de.name, de.name_len,
                    off + offsetof(struct Ext2DirEntry, name));
      if (strncmp(de.name, name, de.name_len) == 0)
        return fs_inode_get(de.inode, 0);
    }
  }

  return NULL;
}

int
fs_dir_link(struct Inode *dp, char *name, unsigned num, uint8_t file_type)
{
  struct Inode *ip;
  struct Ext2DirEntry de;
  int r;

  if ((ip = fs_dir_lookup(dp, name)) != NULL) {
    fs_inode_put(ip);
    return -EEXISTS;
  }

  if (strlen(name) > NAME_MAX)  {
    fs_inode_put(ip);
    return -ENAMETOOLONG;
  }

  de.inode     = num;
  de.name_len  = strlen(name);
  de.rec_len   = offsetof(struct Ext2DirEntry, name) +
                 ROUND_UP(de.name_len, sizeof(uint32_t));
  de.file_type = file_type;

  strncpy(de.name, name, de.name_len);

  if ((r = fs_inode_write(dp, &de, de.rec_len, dp->size)) < 0)
    return r;

  return r == de.rec_len ? 0 : -EIO;
}

/*
 * ----------------------------------------------------------------------------
 * Path names operations
 * ----------------------------------------------------------------------------
 */

static size_t
fs_path_skip(const char *path, char *name, char **next)
{
  const char *end;
  ptrdiff_t n;

  // Skip leading slashes
  while (*path == '/')
    path++;
  
  // This was the last element
  if (*path == '\0')
    return 0;

  // Find the end of the element
  for (end = path; (*end != '\0') && (*end != '/'); end++)
    ;

  n = end - path;
  strncpy(name, path, MIN(n, NAME_MAX));
  name[n] = '\0';

  while (*end == '/')
    end++;

  if (next)
    *next = (char *) end;

  return n;
}

int
fs_path_lookup(const char *path, char *name, int parent, struct Inode **istore)
{
  struct Inode *ip, *next;
  size_t namelen;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the current working directory.
  ip = *path == '/' ? fs_inode_get(2, 0) : fs_inode_dup(my_process()->cwd);

  while ((namelen = fs_path_skip(path, name, (char **) &path)) > 0) {
    if (namelen > NAME_MAX) {
      fs_inode_put(ip);
      return -ENAMETOOLONG;
    }

    fs_inode_lock(ip);

    if ((ip->mode & EXT2_S_IFMASK) != EXT2_S_IFDIR) {
      fs_inode_unlock(ip);
      fs_inode_put(ip);
      return -ENOTDIR;
    }

    if (parent && (*path == '\0')) {
      fs_inode_unlock(ip);

      if (istore)
        *istore = ip;
      return 0;
    }

    if ((next = fs_dir_lookup(ip, name)) == NULL) {
      fs_inode_unlock(ip);
      fs_inode_put(ip);
      return -ENOENT;
    }

    fs_inode_unlock(ip);
    fs_inode_put(ip);

    ip = next;
  }

  if (parent) {
    // File exists!
    fs_inode_put(ip);
    return -EEXISTS;
  }

  if (istore)
    *istore = ip;
  return 0;
}

int
fs_name_lookup(const char *path, struct Inode **ip)
{
  char name[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *ip = fs_inode_get(2, 0);
    return 0;
  }

  return fs_path_lookup(path, name, 0, ip);
}

int
fs_inode_stat(struct Inode *ip, struct stat *buf)
{
  if (!mutex_holding(&ip->mutex))
    panic("caller not holding ip->mutex");

  buf->st_mode  = ip->mode;
  buf->st_ino   = ip->ino;
  buf->st_dev   = ip->dev;
  buf->st_nlink = ip->nlink;
  buf->st_uid   = ip->uid;
  buf->st_gid   = ip->gid;
  buf->st_size  = ip->size;
  buf->st_atime = ip->atime;
  buf->st_mtime = ip->mtime;
  buf->st_ctime = ip->ctime;

  return 0;
}

int
fs_create(const char *path, mode_t mode, struct Inode **istore)
{
  struct Inode *dp, *ip;
  char name[NAME_MAX + 1];
  int r;

  if ((r = fs_path_lookup(path, name, 1, &dp)) < 0)
    return r;

  fs_inode_lock(dp);

  if (fs_dir_lookup(dp, name) != NULL) {
    r = -EEXISTS;
    goto out;
  }

  if ((ip = fs_inode_alloc(mode, dp->dev)) == NULL) {
    r = -ENOMEM;
    goto out;
  }

  fs_inode_lock(ip);

  ip->nlink = 1;
  fs_inode_update(ip);

  if (S_ISDIR(mode)) {
    // Create . and .. entries
    dp->nlink++;
    fs_inode_update(dp);

    if (fs_dir_link(ip, ".", ip->ino, EXT2_S_IFDIR >> 12) < 0)
      panic("Cannot create .");
    if (fs_dir_link(ip, "..", dp->ino, EXT2_S_IFDIR >> 12) < 0)
      panic("Cannot create ..");
  }

  r = fs_dir_link(dp, name, ip->ino, (mode & EXT2_S_IFMASK) >> 12);

  if ((r == 0) && istore) {
    *istore = ip;
  } else {
    fs_inode_unlock(ip);
    fs_inode_put(ip);
  }

out:
  fs_inode_unlock(dp);
  fs_inode_put(dp);

  return r;
}

int
fs_mkdir(const char *path, mode_t mode, struct Inode **istore)
{
  if (mode & ~(S_IRWXU | S_IRWXG | S_IRWXO))
    return -EINVAL;
  return fs_create(path, S_IFDIR | mode, istore);
}

void
fs_init(void)
{
  struct Inode *ip;
  
  spin_init(&inode_cache.lock, "inode_cache");
  list_init(&inode_cache.head);

  for (ip = inode_cache.buf; ip < &inode_cache.buf[INODE_CACHE_SIZE]; ip++) {
    mutex_init(&ip->mutex, "inode");
    list_init(&ip->wait_queue);

    list_add_back(&inode_cache.head, &ip->cache_link);
  }

  fs_read_superblock();
}
