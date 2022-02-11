#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/fs/buf.h>
#include <kernel/fs/ext2.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>

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
    if (fs_inode_read(dir, &de, DE_NAME_OFFSET, off) != DE_NAME_OFFSET)
      panic("Cannot read directory");

    de_len = ROUND_UP(DE_NAME_OFFSET + de.name_len, sizeof(uint32_t));
    if ((de.rec_len - de_len) >= new_len) {
      // Found enough space
      new_de.rec_len = de.rec_len - de_len;
      de.rec_len = de_len;
      
      if (fs_inode_write(dir, &de, DE_NAME_OFFSET, off) != DE_NAME_OFFSET)
        panic("Cannot write directory");
      if (fs_inode_write(dir, &new_de, new_len, off + de_len) != new_len)
        panic("Cannot write directory");

      return 0;
    }
  }

  assert(off % BLOCK_SIZE == 0);

  new_de.rec_len = BLOCK_SIZE;
  dir->size = off + BLOCK_SIZE;

  if (fs_inode_write(dir, &new_de, new_len, off) != new_len)
    panic("Cannot write directory");

  return 0;
}

struct Inode *
fs_dir_lookup(struct Inode *dir, const char *name)
{
  if (!S_ISDIR(dir->mode))
    panic("not a directory");

  return ext2_dir_lookup(dir, name);
}

int
fs_dir_link(struct Inode *dir, char *name, unsigned num, mode_t mode)
{
  struct Inode *ip;

  if ((ip = fs_dir_lookup(dir, name)) != NULL) {
    fs_inode_put(ip);
    return -EEXISTS;
  }

  if (strlen(name) > NAME_MAX)  {
    fs_inode_put(ip);
    return -ENAMETOOLONG;
  }

  return ext2_dir_link(dir, name, num, mode);
}

ssize_t
fs_inode_getdents(struct Inode *dir, void *buf, size_t n, off_t *off)
{
  ssize_t nread, total;
  char *dst;
  
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;
  
  dst = (char *) buf;
  total = 0;
  while ((n > 0) && (nread = ext2_dir_read(dir, dst, n, off)) != 0) {
    if (nread < 0)
      return nread;

    dst   += nread;
    total += nread;
    n     -= nread;
  }

  return total;
}
