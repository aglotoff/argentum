#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/cprintf.h>

#include "dev.h"

static struct Dev {
  ino_t ino;
  char *name;
  mode_t mode;
  dev_t dev;
} devices[] = {
  { 2, ".",    S_IFDIR | 0555, 0x0000 },
  { 2, "..",   S_IFDIR | 0555, 0x0000 },
  { 3, "tty0", S_IFCHR | 0666, 0x0100 },
  { 4, "tty1", S_IFCHR | 0666, 0x0101 },
  { 5, "tty2", S_IFCHR | 0666, 0x0102 },
  { 6, "tty3", S_IFCHR | 0666, 0x0103 },
  { 7, "tty4", S_IFCHR | 0666, 0x0104 },
  { 8, "tty5", S_IFCHR | 0666, 0x0105 },
  { 9, "zero", S_IFCHR | 0666, 0x0202 },
};

#define NDEV  (sizeof(devices) / sizeof devices[0])

static struct Inode *
dev_inode_get(struct FS *fs, ino_t inum)
{
  struct Inode *inode = fs_inode_get(inum, fs->dev);

  if (inode != NULL && inode->fs == NULL) {
    inode->fs = fs;
    inode->extra = NULL;
  }

  return inode;
}

int
dev_inode_read(struct Inode *inode)
{
  if ((inode->ino >= 2) && (inode->ino <= NDEV)) {
    struct Dev *device = &devices[inode->ino - 1];

    inode->mode  = device->mode;
    inode->nlink = 1;
    inode->rdev  = device->dev;
    inode->uid   = 0;
    inode->gid   = 0;
    inode->size  = inode->ino == 2 ? NDEV : 0;
    inode->atime = 0;
    inode->mtime = 0;
    inode->ctime = 0;

    return 0;
  }
  
  return -ENOSYS;
}

int
dev_inode_write(struct Inode *inode)
{
  (void) inode;
  return -ENOSYS;
}

void
dev_inode_delete(struct Inode *inode)
{
  (void) inode;
}

ssize_t
dev_read(struct Inode *inode, void *buf, size_t n, off_t offset)
{
  (void) inode;
  (void) buf;
  (void) n;
  (void) offset;
  return -ENOSYS;
}

ssize_t
dev_write(struct Inode *inode, const void *buf, size_t n, off_t offset)
{
  (void) inode;
  (void) buf;
  (void) n;
  (void) offset;
  return -ENOSYS;
}

int
dev_rmdir(struct Inode *parent, struct Inode *inode)
{
  (void) inode;
  (void) parent;
  return -EROFS;
}

ssize_t
dev_readdir(struct Inode *inode, void *buf, FillDirFunc filldir, off_t offset)
{
  struct Dev *device = &devices[offset];

  if (inode->ino != 2)
    return -ENOTDIR;

  if (offset >= (off_t) NDEV)
    return 0;

  filldir(buf, device->ino, device->name, strlen(device->name));

  return 1;
}

ssize_t
dev_readlink(struct Inode *inode, char *buf, size_t n)
{
  (void) inode;
  (void) buf;
  (void) n;
  return -ENOSYS;
}

int
dev_create(struct Inode *inode, char *name, mode_t mode, struct Inode **store)
{
  (void) inode;
  (void) name;
  (void) mode;
  (void) store;
  return -EROFS;
}

int
dev_mkdir(struct Inode *inode, char *name, mode_t mode, struct Inode **store)
{
  (void) inode;
  (void) name;
  (void) mode;
  (void) store;
  return -EROFS;
}

int
dev_mknod(struct Inode *inode, char *name, mode_t mode, dev_t dev, struct Inode **store)
{
  (void) inode;
  (void) name;
  (void) mode;
  (void) dev;
  (void) store;
  return -EROFS;
}

int
dev_link(struct Inode *parent, char *name, struct Inode *inode)
{
  (void) inode;
  (void) name;
  (void) parent;
  return -EROFS;
}

int
dev_unlink(struct Inode *parent, struct Inode *inode)
{
  (void) inode;
  (void) parent;
  return -EROFS;
}

struct Inode *
dev_lookup(struct Inode *inode, const char *name)
{
  struct Dev *device;
  
  if (inode->ino != 2)
    return NULL;

  for (device = devices; device < &devices[NDEV]; device++)
    if (strcmp(name, device->name) == 0)
      return dev_inode_get(inode->fs, device->ino);

  return NULL;
}

void
dev_trunc(struct Inode *inode, off_t size)
{
  (void) inode;
  (void) size;
}

struct FSOps devfs_ops = {
  .inode_read   = dev_inode_read,
  .inode_write  = dev_inode_write,
  .inode_delete = dev_inode_delete,
  .read         = dev_read,
  .write        = dev_write,
  .trunc        = dev_trunc,
  .rmdir        = dev_rmdir,
  .readdir      = dev_readdir,
  .readlink     = dev_readlink,
  .create       = dev_create,
  .mkdir        = dev_mkdir,
  .mknod        = dev_mknod,
  .link         = dev_link,
  .unlink       = dev_unlink,
  .lookup       = dev_lookup,
};

struct Inode *
dev_mount(dev_t dev)
{
  struct FS *devfs;

  if ((devfs = (struct FS *) k_malloc(sizeof(struct FS))) == NULL)
    panic("cannot allocate FS");
  
  devfs->dev   = dev;
  devfs->extra = NULL;
  devfs->ops   = &devfs_ops;

  return dev_inode_get(devfs, 2);
}
