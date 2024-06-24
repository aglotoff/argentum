#include <kernel/assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include "ext2.h"

/*
 * ----------------------------------------------------------------------------
 * Inode Operations
 * ----------------------------------------------------------------------------
 */

static struct Inode *
ext2_inode_get(struct FS *fs, ino_t inum)
{
  struct Inode *inode = fs_inode_get(inum, fs->dev);

  if (inode != NULL && inode->fs == NULL) {
    inode->fs = fs;
    if ((inode->extra = k_malloc(sizeof(struct Ext2InodeExtra))) == NULL)
      panic("TODO");
  }

  return inode;
}

int
ext2_inode_create(struct Inode *dirp, char *name, mode_t mode, dev_t rdev,
            struct Inode **istore)
{
  struct Inode *ip;
  uint32_t inum;
  int r;
  
  if ((r = ext2_inode_alloc((struct Ext2SuperblockData *) (dirp->fs->extra), mode, rdev, dirp->dev, &inum, dirp->ino)) < 0)
    return r;

  if ((ip = ext2_inode_get(dirp->fs, inum)) == NULL)
    return -ENOMEM;

  fs_inode_lock(ip);

  ip->uid = process_current()->euid;
  ip->gid = dirp->gid;

  if ((r = ext2_link(dirp, name, ip)))
    panic("Cannot create link");

  ip->ctime = ip->mtime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  *istore = ip;

  return 0;
}

int
ext2_create(struct Inode *dirp, char *name, mode_t mode,
                  struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_inode_create(dirp, name, mode, 0, &ip)) != 0)
    return r;

  ip->nlink = 1;
  ip->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;

  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
  dirp->flags |= FS_INODE_DIRTY;

  return 0;
}

int
ext2_mkdir(struct Inode *dirp, char *name, mode_t mode,
                 struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if (dirp->nlink >= LINK_MAX)
    return -EMLINK;

  if ((r = ext2_inode_create(dirp, name, mode, 0, &ip)) != 0)
    return r;

  // Create the "." entry
  if (ext2_link(ip, ".", ip) < 0)
    panic("Cannot create .");

  ip->nlink = 1;
  ip->flags |= FS_INODE_DIRTY;

  // Create the ".." entry
  if (ext2_link(ip, "..", dirp) < 0)
    panic("Cannot create ..");

  dirp->nlink++;
  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
  dirp->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;

  return 0;
}

int
ext2_mknod(struct Inode *dirp, char *name, mode_t mode, dev_t dev,
                 struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_inode_create(dirp, name, mode, dev, &ip)) != 0)
    return r;

  ip->nlink = 1;
  ip->rdev = dev;
  ip->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;

  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
  dirp->flags |= FS_INODE_DIRTY;

  return 0;
}

#define DE_NAME_OFFSET    offsetof(struct Ext2DirEntry, name)

ssize_t
ext2_dirent_read(struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  ssize_t ret;

  ret = ext2_read(dir, de, DE_NAME_OFFSET, off);
  if (ret != DE_NAME_OFFSET)
    panic("Cannot read directory");

  ret = ext2_read(dir, de->name, de->name_len, off + ret);
  if (ret != de->name_len)
    panic("Cannot read directory");

  dir->atime  = rtc_get_time();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

ssize_t
ext2_dirent_write(struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  size_t ret;

  ret = ext2_write(dir, de, DE_NAME_OFFSET + de->name_len, off);
  if (ret != (DE_NAME_OFFSET + de->name_len))
    panic("Cannot read directory");

  return 0;
}

struct Inode *
ext2_lookup(struct Inode *dirp, const char *name)
{
  struct Ext2DirEntry de;
  off_t off;
  size_t name_len;

  if (!S_ISDIR(dirp->mode))
    panic("not a directory");

  name_len = strlen(name);

  for (off = 0; off < dirp->size; off += de.rec_len) {
    ext2_dirent_read(dirp, &de, off);

    if (de.inode == 0)
      continue;

    if (de.name_len != name_len)
      continue;

    if (strncmp(de.name, name, de.name_len) == 0)
      return ext2_inode_get(dirp->fs, de.inode);
  }

  return NULL;
}

int
ext2_link(struct Inode *dir, char *name, struct Inode *ip)
{
  struct Inode *ip2;
  struct Ext2DirEntry de, new_de;
  off_t off;
  ssize_t name_len, de_len, new_len;
  uint8_t file_type;
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (dir->fs->extra);

  if ((ip2 = ext2_lookup(dir, name)) != NULL) {
    fs_inode_put(ip2);
    return -EEXIST;
  }

  name_len = strlen(name);
  if (name_len > NAME_MAX)
    return -ENAMETOOLONG;

  switch (ip->mode & S_IFMT) {
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

  new_de.inode     = ip->ino;
  new_de.name_len  = name_len;
  new_de.file_type = file_type;
  strncpy(new_de.name, name, ROUND_UP(name_len, sizeof(uint32_t)));

  for (off = 0; off < dir->size; off += de.rec_len) {
    ext2_dirent_read(dir, &de, off);

    if (de.inode == 0) {
      if (de.rec_len < new_len)
        continue;
      
      // Reuse an empty entry
      new_de.rec_len = de.rec_len;

      ip->ctime = rtc_get_time();
      ip->nlink++;
      ip->flags |= FS_INODE_DIRTY;

      return ext2_dirent_write(dir, &new_de, off);
    }

    de_len = ROUND_UP(DE_NAME_OFFSET + de.name_len, sizeof(uint32_t));

    if ((de.rec_len - de_len) >= new_len) {
      // Found enough space
      new_de.rec_len = de.rec_len - de_len;
      de.rec_len = de_len;

      ip->ctime = rtc_get_time();
      ip->nlink++;
      ip->flags |= FS_INODE_DIRTY;

      ext2_dirent_write(dir, &de, off);
      ext2_dirent_write(dir, &new_de, off + de_len);

      return 0;
    }
  }

  assert(off % sb->block_size == 0);

  new_de.rec_len = sb->block_size;
  dir->size = off + sb->block_size;

  ip->ctime = rtc_get_time();
  ip->nlink++;
  ip->flags |= FS_INODE_DIRTY;

  ext2_dirent_write(dir, &new_de, off);

  return 0;
}

static int
ext2_dir_empty(struct Inode *dir)
{
  struct Ext2DirEntry de;
  off_t off;

  for (off = 0; off < dir->size; off += de.rec_len) {
    ext2_dirent_read(dir, &de, off);

    if (de.inode == 0)
      continue;

    if ((de.name_len == 1) && (strncmp(de.name, ".", de.name_len) == 0))
      continue;
    if ((de.name_len == 2) && (strncmp(de.name, "..", de.name_len) == 0))
      continue;

    return 0;
  }

  return 1;
}

int
ext2_unlink(struct Inode *dir, struct Inode *ip)
{
  struct Ext2DirEntry de;
  off_t off, prev_off;
  size_t rec_len;

  if (dir->ino == ip->ino)
    return -EBUSY;

  for (prev_off = off = 0; off < dir->size; prev_off = off, off += de.rec_len) {
    ext2_dirent_read(dir, &de, off);

    if (de.inode != ip->ino)
      continue;

    if (prev_off == off) {
      // Removed the first entry - create an unused entry
      memset(de.name, 0, de.name_len);
      de.name_len  = 0;
      de.file_type = 0;
      de.inode     = 0;

      ext2_dirent_write(dir, &de, off);
    } else {
      // Update length of the previous entry
      rec_len = de.rec_len;

      ext2_dirent_read(dir, &de, prev_off);
      de.rec_len += rec_len;
      ext2_dirent_write(dir, &de, prev_off);
    }

    if (--ip->nlink > 0)
      ip->ctime = rtc_get_time();
    ip->flags |= FS_INODE_DIRTY;

    return 0;
  }

  return -ENOENT;
}

int
ext2_rmdir(struct Inode *dir, struct Inode *ip)
{
  int r;

  if (!ext2_dir_empty(ip))
    return -ENOTEMPTY;

  if ((r = ext2_unlink(dir, ip)) < 0)
    return r;

  dir->nlink--;
  dir->ctime = dir->mtime = rtc_get_time();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

void
ext2_inode_delete(struct Inode *ip)
{
  ext2_trunc(ip, 0);

  ip->mode = 0;
  ip->size = 0;
  ext2_inode_write(ip);

  ext2_inode_free((struct Ext2SuperblockData *) (ip->fs->extra), ip->dev, ip->ino);
}

/*
 * ----------------------------------------------------------------------------
 * Superblock operations
 * ----------------------------------------------------------------------------
 */

#define EXT2_SB_NO            1
#define EXT2_SB_OFFSET        0

void
ext2_sb_sync(struct Ext2SuperblockData *sb, dev_t dev)
{
  struct Ext2Superblock *raw;
  struct Buf *buf;

  k_mutex_lock(&sb->mutex);

  sb->wtime = rtc_get_time();

  if ((buf = buf_read(1, 1024, dev)) == NULL)
    panic("cannot read the superblock");

  raw = (struct Ext2Superblock *) &buf->data[EXT2_SB_OFFSET];

  raw->wtime             = sb->wtime;
  raw->free_blocks_count = sb->free_blocks_count;

  buf->flags = BUF_DIRTY;

  buf_release(buf);

  k_mutex_unlock(&sb->mutex);
}

struct FSOps ext2fs_ops = {
  .inode_read   = ext2_inode_read,
  .inode_write  = ext2_inode_write,
  .inode_delete = ext2_inode_delete,
  .read         = ext2_read,
  .write        = ext2_write,
  .trunc        = ext2_trunc,
  .rmdir        = ext2_rmdir,
  .readdir      = ext2_readdir,
  .readlink     = ext2_readlink,
  .create       = ext2_create,
  .mkdir        = ext2_mkdir,
  .mknod        = ext2_mknod,
  .link         = ext2_link,
  .unlink       = ext2_unlink,
  .lookup       = ext2_lookup,
};

struct Inode *
ext2_mount(dev_t dev)
{
  struct Buf *buf;
  struct Ext2Superblock *raw;
  struct Ext2SuperblockData *sb;
  struct FS *ext2fs;

  if ((ext2fs = (struct FS *) k_malloc(sizeof(struct FS))) == NULL)
    panic("cannot allocate FS");
  if ((sb = (struct Ext2SuperblockData *) k_malloc(sizeof(struct Ext2SuperblockData))) == NULL)
    panic("cannt allocate superblock");

  k_mutex_init(&sb->mutex, "ext2_sb_mutex");
  
  if ((buf = buf_read(1, 1024, dev)) == NULL)
    panic("cannot read the superblock");

  raw = (struct Ext2Superblock *) &buf->data[EXT2_SB_OFFSET];

  sb->block_count       = raw->block_count;
  sb->inodes_count      = raw->inodes_count;
  sb->r_blocks_count    = raw->r_blocks_count;
  sb->free_blocks_count = raw->free_blocks_count;
  sb->log_block_size    = raw->log_block_size;
  sb->blocks_per_group  = raw->blocks_per_group;
  sb->inodes_per_group  = raw->inodes_per_group;
  sb->wtime             = raw->wtime;
  sb->inode_size        = raw->inode_size;

  buf_release(buf);

  sb->block_size = 1024 << sb->log_block_size;

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          sb->block_count * sb->block_size / (1024 * 1024),
          sb->inodes_count, sb->block_count);

  // TODO: update mtime, mnt_count, state, last_mounted

  ext2fs->dev   = dev;
  ext2fs->extra = sb;
  ext2fs->ops   = &ext2fs_ops;

  return ext2_inode_get(ext2fs, 2);
}

ssize_t
ext2_readdir(struct Inode *dir, void *buf, FillDirFunc filldir, off_t off)
{
  struct Ext2DirEntry de;
  ssize_t nread;

  assert(S_ISDIR(dir->mode));

  if (off >= dir->size)
    return 0;

  if ((nread = ext2_dirent_read(dir, &de, off)) < 0)
    return nread;

  filldir(buf, de.inode, de.name, de.name_len);

  return de.rec_len;
}

#define MAX_FAST_SYMLINK_NAMELEN  60

ssize_t
ext2_readlink(struct Inode *inode, char *buf, size_t n)
{
  struct Ext2InodeExtra *extra = (struct Ext2InodeExtra *) inode->extra;

  assert(IS_ISLNK(inode->mode));

  if ((inode->size <= MAX_FAST_SYMLINK_NAMELEN) && (extra->blocks == 0)) {
    ssize_t nread = MIN((size_t) inode->size, n);
    memmove(buf, extra->block, nread);
    return nread;
  }

  return ext2_read(inode, buf, n, 0);
}
