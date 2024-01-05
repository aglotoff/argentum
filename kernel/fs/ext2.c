#include <kernel/assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include "ext2.h"

int ext2_sb_dirty;
struct Ext2Superblock ext2_sb;
struct KMutex ext2_sb_mutex;

/*
 * ----------------------------------------------------------------------------
 * Inode allocator
 * ----------------------------------------------------------------------------
 */

// Block descriptors begin at block 2
#define GD_BLOCKS_BASE  2

/*
 * ----------------------------------------------------------------------------
 * Inode Operations
 * ----------------------------------------------------------------------------
 */

static struct Inode *
fs_inode_alloc(mode_t mode, dev_t rdev, dev_t dev, ino_t parent)
{
  uint32_t inum;

  if (ext2_inode_alloc(mode, rdev, dev, &inum, parent) < 0)
    return NULL;

  return fs_inode_get(inum, dev);
}

int
ext2_create(struct Inode *dirp, char *name, mode_t mode, dev_t rdev,
            struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((ip = fs_inode_alloc(mode, rdev, dirp->dev, dirp->ino)) == NULL)
    return -ENOMEM;

  fs_inode_lock(ip);

  ip->uid = process_current()->euid;
  ip->gid = dirp->gid;

  if ((r = ext2_inode_link(dirp, name, ip)))
    panic("Cannot create link");

  ip->ctime = ip->mtime = rtc_get_time();
  ip->flags |= FS_INODE_DIRTY;

  *istore = ip;

  return 0;
}

int
ext2_inode_create(struct Inode *dirp, char *name, mode_t mode,
                  struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_create(dirp, name, mode, 0, &ip)) != 0)
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
ext2_inode_mkdir(struct Inode *dirp, char *name, mode_t mode,
                 struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if (dirp->nlink >= LINK_MAX)
    return -EMLINK;

  if ((r = ext2_create(dirp, name, mode, 0, &ip)) != 0)
    return r;

  // Create the "." entry
  if (ext2_inode_link(ip, ".", ip) < 0)
    panic("Cannot create .");

  ip->nlink = 1;
  ip->flags |= FS_INODE_DIRTY;

  // Create the ".." entry
  if (ext2_inode_link(ip, "..", dirp) < 0)
    panic("Cannot create ..");

  dirp->nlink++;
  dirp->atime = dirp->ctime = dirp->mtime = rtc_get_time();
  dirp->flags |= FS_INODE_DIRTY;

  assert(istore != NULL);
  *istore = ip;

  return 0;
}

int
ext2_inode_mknod(struct Inode *dirp, char *name, mode_t mode, dev_t dev,
                 struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_create(dirp, name, mode, dev, &ip)) != 0)
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
ext2_inode_lookup(struct Inode *dirp, const char *name)
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
      return fs_inode_get(de.inode, 0);
  }

  return NULL;
}

int
ext2_inode_link(struct Inode *dir, char *name, struct Inode *ip)
{
  struct Inode *ip2;
  struct Ext2DirEntry de, new_de;
  off_t off;
  ssize_t name_len, de_len, new_len;
  uint8_t file_type;

  if ((ip2 = ext2_inode_lookup(dir, name)) != NULL) {
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

  assert(off % BLOCK_SIZE == 0);

  new_de.rec_len = BLOCK_SIZE;
  dir->size = off + BLOCK_SIZE;

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
ext2_inode_unlink(struct Inode *dir, struct Inode *ip)
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
ext2_inode_rmdir(struct Inode *dir, struct Inode *ip)
{
  int r;

  if (!ext2_dir_empty(ip))
    return -ENOTEMPTY;

  if ((r = ext2_inode_unlink(dir, ip)) < 0)
    return r;

  dir->nlink--;
  dir->ctime = dir->mtime = rtc_get_time();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

void
ext2_delete_inode(struct Inode *ip)
{
  ext2_inode_trunc(ip, 0);

  ip->mode = 0;
  ip->size = 0;
  ext2_write_inode(ip);

  ext2_inode_free(ip->dev, ip->ino);
}

/*
 * ----------------------------------------------------------------------------
 * Superblock operations
 * ----------------------------------------------------------------------------
 */

void
ext2_sb_sync(void)
{
  struct Buf *buf;

  kmutex_lock(&ext2_sb_mutex);

  if ((buf = buf_read(1, 0)) == NULL)
    panic("cannot read the superblock");

  ext2_sb.wtime = rtc_get_time();

  memcpy(buf->data, &ext2_sb, sizeof(ext2_sb));
  buf->flags = BUF_DIRTY;

  buf_release(buf);

  kmutex_unlock(&ext2_sb_mutex);
}

struct Inode *
ext2_mount(void)
{
  struct Buf *buf;

  kmutex_init(&ext2_sb_mutex, "ext2_sb_mutex");
  
  if ((buf = buf_read(1, 0)) == NULL)
    panic("cannot read the superblock");

  memcpy(&ext2_sb, buf->data, sizeof(ext2_sb));
  buf_release(buf);

  if (ext2_sb.log_block_size != 0)
    panic("only block sizes of 1024 are supported!");

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          ext2_sb.block_count * BLOCK_SIZE / (1024 * 1024),
          ext2_sb.inodes_count, ext2_sb.block_count);

  // TODO: update mtime, mnt_count, state, last_mounted

  return fs_inode_get(2, 0);
}

ssize_t
ext2_readdir(struct Inode *dir, void *buf, FillDirFunc filldir, off_t off)
{
  struct Ext2DirEntry de;
  ssize_t nread;

  if (!S_ISDIR(dir->mode))
    panic("not a directory");

  if (off >= dir->size)
    return 0;

  if ((nread = ext2_dirent_read(dir, &de, off)) < 0)
    return nread;

  filldir(buf, de.inode, de.name, de.name_len);

  return de.rec_len;
}
