#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <cprintf.h>
#include <fs/buf.h>
#include <fs/ext2.h>
#include <fs/fs.h>
#include <types.h>

#define DE_NAME_OFFSET    offsetof(struct Ext2DirEntry, name)

ssize_t
ext2_dirent_read(struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  ssize_t ret;

  ret = ext2_inode_read(dir, de, DE_NAME_OFFSET, off);
  if (ret != DE_NAME_OFFSET)
    panic("Cannot read directory");

  ret = ext2_inode_read(dir, de->name, de->name_len, off + ret);
  if (ret != de->name_len)
    panic("Cannot read directory");

  return 0;
}

ssize_t
ext2_dirent_write(struct Inode *dir, struct Ext2DirEntry *de, off_t off)
{
  size_t ret;

  ret = ext2_inode_write(dir, de, DE_NAME_OFFSET + de->name_len, off);
  if (ret != (DE_NAME_OFFSET + de->name_len))
    panic("Cannot read directory");

  return 0;
}

ssize_t
ext2_dir_iterate(struct Inode *dir, void *buf, size_t n, off_t *off)
{
  struct Ext2DirEntry de;
  struct dirent *dp;
  ssize_t nread;

  if (*off >= dir->size)
    return 0;

  if ((nread = ext2_dirent_read(dir, &de, *off)) < 0)
    return nread;

  if ((size_t) nread > n)
    return -EINVAL;

  dp = (struct dirent *) buf;
  dp->d_reclen  = de.name_len + offsetof(struct dirent, d_name);
  dp->d_ino     = de.inode;
  dp->d_off     = *off + de.rec_len;
  dp->d_namelen = de.name_len;
  dp->d_type    = de.file_type;
  memmove(&dp->d_name[0], de.name, de.name_len);

  *off += de.rec_len;

  return dp->d_reclen;
}
