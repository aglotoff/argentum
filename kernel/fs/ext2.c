#include <kernel/core/assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/console.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/types.h>
#include <kernel/time.h>
#include <kernel/vmspace.h>

#include "ext2.h"

/*
 * ----------------------------------------------------------------------------
 * Inode Operations
 * ----------------------------------------------------------------------------
 */

struct Inode *
ext2_inode_get(struct FS *fs, ino_t inum)
{
  struct Inode *inode = fs_inode_get(inum, fs->dev);

  if (inode != NULL && inode->fs == NULL) {
    inode->fs = fs;
    if ((inode->extra = k_malloc(sizeof(struct Ext2InodeExtra))) == NULL)
      k_panic("TODO");
  }

  return inode;
}

int
ext2_inode_create(struct Process *process, struct Inode *dirp, char *name, mode_t mode, dev_t rdev,
            struct Inode **istore)
{
  struct Inode *ip;
  uint32_t inum;
  int r;
  uid_t euid = process == NULL ? 0 : process->euid;

  if ((r = ext2_inode_alloc((struct Ext2SuperblockData *) (dirp->fs->extra), mode, rdev, dirp->dev, &inum, dirp->ino)) < 0)
    return r;

  if ((ip = ext2_inode_get(dirp->fs, inum)) == NULL)
    return -ENOMEM;

  fs_inode_lock(ip);

  ip->uid = euid;
  ip->gid = dirp->gid;

  if ((r = ext2_link(process, dirp, name, ip)))
    k_panic("Cannot create link");

  ip->ctime = ip->mtime = time_get_seconds();
  ip->flags |= FS_INODE_DIRTY;

  *istore = ip;

  return 0;
}

int
ext2_create(struct Process *process, struct Inode *dirp, char *name, mode_t mode,
            struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_inode_create(process, dirp, name, mode, 0, &ip)) != 0)
    return r;

  ip->nlink = 1;
  ip->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(ip);

  k_assert(istore != NULL);
  *istore = ip;

  dirp->atime = dirp->ctime = dirp->mtime = time_get_seconds();
  dirp->flags |= FS_INODE_DIRTY;

  return 0;
}

int
ext2_mkdir(struct Process *process, struct Inode *dirp, char *name, mode_t mode,
           struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if (dirp->nlink >= LINK_MAX)
    return -EMLINK;

  if ((r = ext2_inode_create(process, dirp, name, mode, 0, &ip)) != 0)
    return r;

  // Create the "." entry
  if (ext2_link(process, ip, ".", ip) < 0)
    k_panic("Cannot create .");

  ip->nlink = 2;
  ip->flags |= FS_INODE_DIRTY;

  // Create the ".." entry
  if (ext2_link(process, ip, "..", dirp) < 0)
    k_panic("Cannot create ..");

  fs_inode_unlock(ip);

  dirp->nlink++;
  dirp->atime = dirp->ctime = dirp->mtime = time_get_seconds();
  dirp->flags |= FS_INODE_DIRTY;

  k_assert(istore != NULL);
  *istore = ip;

  return 0;
}

int
ext2_mknod(struct Process *process, struct Inode *dirp, char *name, mode_t mode,
           dev_t dev, struct Inode **istore)
{
  struct Inode *ip;
  int r;

  if ((r = ext2_inode_create(process, dirp, name, mode, dev, &ip)) != 0)
    return r;

  ip->nlink = 1;
  ip->rdev = dev;
  ip->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(ip);

  k_assert(istore != NULL);
  *istore = ip;

  dirp->atime = dirp->ctime = dirp->mtime = time_get_seconds();
  dirp->flags |= FS_INODE_DIRTY;

  return 0;
}

#define DE_NAME_OFFSET    offsetof(struct Ext2DirEntry, name)

ssize_t
ext2_dirent_read(struct Process *process, struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  ssize_t ret;

  ret = ext2_read(process, dir, (uintptr_t) de, DE_NAME_OFFSET, off);
  if (ret != DE_NAME_OFFSET)
    k_panic("Cannot read directory");

  ret = ext2_read(process, dir, (uintptr_t) de->name, de->name_len, off + ret);
  if (ret != de->name_len)
    k_panic("Cannot read directory");

  dir->atime  = time_get_seconds();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

ssize_t
ext2_dirent_write(struct Process *process, struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  size_t ret;

  ret = ext2_write(process, dir, (uintptr_t) de, DE_NAME_OFFSET + de->name_len, off);
  if (ret != (DE_NAME_OFFSET + de->name_len))
    k_panic("Cannot read directory");

  return 0;
}

struct Inode *
ext2_lookup(struct Process *process, struct Inode *dirp, const char *name)
{
  struct Ext2DirEntry de;
  off_t off;
  size_t name_len;

  if (!S_ISDIR(dirp->mode))
    k_panic("not a directory");

  name_len = strlen(name);

  for (off = 0; off < dirp->size; off += de.rec_len) {
    ext2_dirent_read(process, dirp, &de, off);

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
ext2_link(struct Process *process, struct Inode *dir, char *name, struct Inode *inode)
{
  struct Inode *existing_inode;
  struct Ext2DirEntry de, new_de;
  off_t off;
  ssize_t name_len, de_len, new_len;
  uint8_t file_type;
  struct Ext2SuperblockData *sb = (struct Ext2SuperblockData *) (dir->fs->extra);

  if ((existing_inode = ext2_lookup(process, dir, name)) != NULL) {
    fs_inode_put(existing_inode);
    return -EEXIST;
  }

  name_len = strlen(name);
  if (name_len > NAME_MAX)
    return -ENAMETOOLONG;

  switch (inode->mode & S_IFMT) {
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

  new_de.inode     = inode->ino;
  new_de.name_len  = name_len;
  new_de.file_type = file_type;
  strncpy(new_de.name, name, ROUND_UP(name_len, sizeof(uint32_t)));

  for (off = 0; off < dir->size; off += de.rec_len) {
    ext2_dirent_read(process, dir, &de, off);

    if (de.inode == 0) {
      if (de.rec_len < new_len)
        continue;
      
      // Reuse an empty entry
      new_de.rec_len = de.rec_len;

      inode->ctime = time_get_seconds();
      inode->nlink++;
      inode->flags |= FS_INODE_DIRTY;

      return ext2_dirent_write(process, dir, &new_de, off);
    }

    de_len = ROUND_UP(DE_NAME_OFFSET + de.name_len, sizeof(uint32_t));

    if ((de.rec_len - de_len) >= new_len) {
      // Found enough space
      new_de.rec_len = de.rec_len - de_len;
      de.rec_len = de_len;

      inode->ctime = time_get_seconds();
      inode->nlink++;
      inode->flags |= FS_INODE_DIRTY;

      ext2_dirent_write(process, dir, &de, off);
      ext2_dirent_write(process, dir, &new_de, off + de_len);

      return 0;
    }
  }

  k_assert(off % sb->block_size == 0);

  new_de.rec_len = sb->block_size;
  dir->size = off + sb->block_size;

  inode->ctime = time_get_seconds();
  inode->nlink++;
  inode->flags |= FS_INODE_DIRTY;

  ext2_dirent_write(process, dir, &new_de, off);

  return 0;
}

// A directory is considered empty only if it only consists of unused entries
// and a self and a parent entry
static int
ext2_dir_empty(struct Process *process, struct Inode *dir)
{
  struct Ext2DirEntry de;
  off_t off;

  for (off = 0; off < dir->size; off += de.rec_len) {
    ext2_dirent_read(process, dir, &de, off);

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
ext2_unlink(struct Process *process, struct Inode *dir, struct Inode *ip, const char *name)
{
  struct Ext2DirEntry de;
  off_t off, prev_off;
  size_t rec_len;
  size_t name_len;

  if ((dir->ino == ip->ino) && (strcmp(name, ".") != 0))
    return -EBUSY;

  name_len = strlen(name);

  for (prev_off = off = 0; off < dir->size; prev_off = off, off += de.rec_len) {
    ext2_dirent_read(process, dir, &de, off);

    if (de.inode != ip->ino)
      continue;

    if ((de.name_len != name_len) || (strncmp(de.name, name, de.name_len) != 0))
      continue;

    if (prev_off == off) {
      // Removed the first entry - create an unused entry
      memset(de.name, 0, de.name_len);
      de.name_len  = 0;
      de.file_type = 0;
      de.inode     = 0;

      ext2_dirent_write(process, dir, &de, off);
    } else {
      // Update length of the previous entry
      rec_len = de.rec_len;

      ext2_dirent_read(process, dir, &de, prev_off);
      de.rec_len += rec_len;
      ext2_dirent_write(process, dir, &de, prev_off);
    }

    if (--ip->nlink > 0)
      ip->ctime = time_get_seconds();
    
    ip->flags |= FS_INODE_DIRTY;

    return 0;
  }

  return -ENOENT;
}

int
ext2_rmdir(struct Process *process,
           struct Inode *dir,
           struct Inode *inode,
           const char *name)
{
  int r;

  k_assert(strcmp(name, ".") != 0);
  k_assert(strcmp(name, "..") != 0);

  if (!ext2_dir_empty(process, inode))
    return -ENOTEMPTY;

  if ((r = ext2_unlink(process, inode, inode, ".")) < 0)
    return r;
  if ((r = ext2_unlink(process, inode, dir, "..")) < 0)
    return r;

  if ((r = ext2_unlink(process, dir, inode, name)) < 0)
    return r;

  dir->nlink--;
  dir->ctime = dir->mtime = time_get_seconds();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

void
ext2_inode_delete(struct Process *process, struct Inode *inode)
{
  if (!S_ISLNK(inode->mode) || (inode->size > MAX_FAST_SYMLINK_NAMELEN))
    ext2_trunc(process, inode, 0);

  inode->mode = 0;
  inode->size = 0;
  ext2_inode_write(process, inode);

  ext2_inode_free((struct Ext2SuperblockData *) (inode->fs->extra), inode->dev, inode->ino);
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

  if (k_mutex_lock(&sb->mutex) < 0)
    k_panic("TODO");

  sb->wtime = time_get_seconds();

  if ((buf = buf_read(1, 1024, dev)) == NULL)
    k_panic("cannot read the superblock");

  raw = (struct Ext2Superblock *) &buf->data[EXT2_SB_OFFSET];

  raw->wtime             = sb->wtime;
  raw->free_blocks_count = sb->free_blocks_count;
  raw->free_inodes_count = sb->free_inodes_count;

  buf_write(buf);

  k_mutex_unlock(&sb->mutex);
}

ssize_t
ext2_readdir(struct Process *process, struct Inode *dir, void *buf,
             FillDirFunc filldir, off_t off)
{
  struct Ext2DirEntry de;
  ssize_t nread;

  k_assert(S_ISDIR(dir->mode));

  if (off >= dir->size)
    return 0;

  if ((nread = ext2_dirent_read(process, dir, &de, off)) < 0)
    return nread;

  filldir(buf, de.inode, de.name, de.name_len);

  return de.rec_len;
}

ssize_t
ext2_readlink(struct Process *process, struct Inode *inode, uintptr_t va, size_t n)
{
  struct Ext2InodeExtra *extra = (struct Ext2InodeExtra *) inode->extra;
  int r;

  k_assert(S_ISLNK(inode->mode));

  if ((inode->size <= MAX_FAST_SYMLINK_NAMELEN) && (extra->blocks == 0)) {
    ssize_t nread = MIN((size_t) inode->size, n);

    if ((r = vm_space_copy_out(process, extra->block, va, n)) < 0)
      return r;

    return nread;
  }

  return ext2_read(process, inode, va, n, 0);
}

int
ext2_symlink(struct Process *process,
             struct Inode *dir,
             char *name,
             mode_t mode,
             const char *link_path,
             struct Inode **istore)
{
  struct Inode *ip;
  int r;
  size_t path_len;

  path_len = strlen(link_path);

  if ((r = ext2_inode_create(process, dir, name, mode, 0, &ip)) != 0)
    return r;

  ip->nlink = 1;

  if (path_len <= MAX_FAST_SYMLINK_NAMELEN) {
    struct Ext2InodeExtra *extra = (struct Ext2InodeExtra *) ip->extra;

    if ((r = vm_space_copy_in(process, extra->block, (uintptr_t) link_path, path_len)) < 0) {
      fs_inode_unlock(ip);
      return r;
    }

    ip->size = path_len;
  } else {
    if ((r = ext2_write(process, ip, (uintptr_t) link_path, path_len, 0)) < 0) {
      fs_inode_unlock(ip);
      return r;
    }
  }

  ip->flags |= FS_INODE_DIRTY;

  fs_inode_unlock(ip);

  k_assert(istore != NULL);
  *istore = ip;

  dir->atime = dir->ctime = dir->mtime = time_get_seconds();
  dir->flags |= FS_INODE_DIRTY;

  return 0;
}

struct FSOps ext2fs_ops = {
  .inode_get    = ext2_inode_get,
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
  .symlink      = ext2_symlink,
};

ino_t
ext2_mount(dev_t dev, struct FS **fs_store)
{
  struct Buf *buf;
  struct Ext2Superblock *raw;
  struct Ext2SuperblockData *sb;
  struct FS *ext2fs;
  struct Inode *root;

  if ((sb = (struct Ext2SuperblockData *) k_malloc(sizeof(struct Ext2SuperblockData))) == NULL)
    k_panic("cannt allocate superblock");
  if ((ext2fs = fs_create_service("ext2", dev, sb, &ext2fs_ops)) == NULL)
    k_panic("cannot allocate FS");

  k_mutex_init(&sb->mutex, "ext2_sb_mutex");
  
  if ((buf = buf_read(1, 1024, dev)) == NULL)
    k_panic("cannot read the superblock");

  raw = (struct Ext2Superblock *) &buf->data[EXT2_SB_OFFSET];

  sb->block_count       = raw->block_count;
  sb->inodes_count      = raw->inodes_count;
  sb->r_blocks_count    = raw->r_blocks_count;
  sb->free_blocks_count = raw->free_blocks_count;
  sb->free_inodes_count = raw->free_inodes_count;
  sb->log_block_size    = raw->log_block_size;
  sb->blocks_per_group  = raw->blocks_per_group;
  sb->inodes_per_group  = raw->inodes_per_group;
  sb->wtime             = raw->wtime;
  sb->inode_size        = raw->inode_size;

  buf_release(buf);

  sb->block_size = 1024 << sb->log_block_size;

  cprintf("FS size = %dM, %d inodes (%d free), %d blocks (%d free)\n",
          sb->block_count * sb->block_size / (1024 * 1024),
          sb->inodes_count, sb->free_inodes_count,
          sb->block_count, sb->free_blocks_count);

  // TODO: update mtime, mnt_count, state, last_mounted

  root = ext2_inode_get(ext2fs, 2);

  if (fs_store)
    *fs_store = ext2fs;

  return root->ino;
}
