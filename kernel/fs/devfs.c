#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/console.h>
#include <kernel/dev.h>

#include "devfs.h"

static struct DevfsNode {
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
  { 10, "null", S_IFCHR | 0666, 0x0203 },
  { 11, "tty", S_IFCHR | 0666, 0x0300 },
};

#define NDEV  (sizeof(devices) / sizeof devices[0])

static struct Inode *
devfs_inode_get(struct FS *fs, ino_t inum)
{
  struct Inode *inode = fs_inode_get(inum, fs->dev);

  if (inode != NULL && inode->fs == NULL) {
    inode->fs = fs;
    inode->extra = NULL;
  }

  return inode;
}

int
devfs_inode_read(struct Inode *inode)
{
  if ((inode->ino >= 2) && (inode->ino <= NDEV)) {
    struct DevfsNode *device = &devices[inode->ino - 1];

    assert(inode->dev == 1);

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
devfs_inode_write(struct Inode *inode)
{
  assert(inode->dev == 1);
  return -ENOSYS;
}

void
devfs_inode_delete(struct Inode *inode)
{
  assert(inode->dev == 1);
}

ssize_t
devfs_read(struct Inode *inode, uintptr_t va, size_t n, off_t offset)
{
  assert(inode->dev == 1);
  (void) va;
  (void) n;
  (void) offset;
  return -ENOSYS;
}

ssize_t
devfs_write(struct Inode *inode, uintptr_t va, size_t n, off_t offset)
{
  (void) inode;
  (void) va;
  (void) n;
  (void) offset;
  return -ENOSYS;
}

int
devfs_rmdir(struct Inode *parent, struct Inode *inode)
{
  assert(inode->dev == 1);
  (void) parent;
  return -EROFS;
}

ssize_t
devfs_readdir(struct Inode *inode, void *buf, FillDirFunc filldir, off_t offset)
{
  struct DevfsNode *device = &devices[offset];

  assert(inode->dev == 1);

  if (inode->ino != 2)
    return -ENOTDIR;

  if (offset >= (off_t) NDEV)
    return 0;

  filldir(buf, device->ino, device->name, strlen(device->name));

  return 1;
}

ssize_t
devfs_readlink(struct Inode *inode, char *buf, size_t n)
{
  assert(inode->dev == 1);
  (void) buf;
  (void) n;
  return -ENOSYS;
}

int
devfs_create(struct Inode *inode, char *name, mode_t mode, struct Inode **store)
{
  assert(inode->dev == 1);
  (void) name;
  (void) mode;
  (void) store;
  return -EROFS;
}

int
devfs_mkdir(struct Inode *inode, char *name, mode_t mode, struct Inode **store)
{
  assert(inode->dev == 1);
  (void) name;
  (void) mode;
  (void) store;
  return -EROFS;
}

int
devfs_mknod(struct Inode *inode, char *name, mode_t mode, dev_t dev, struct Inode **store)
{
  assert(inode->dev == 1);
  (void) name;
  (void) mode;
  (void) dev;
  (void) store;
  return -EROFS;
}

int
devfs_link(struct Inode *parent, char *name, struct Inode *inode)
{
  assert(inode->dev == 1);
  (void) name;
  (void) parent;
  return -EROFS;
}

int
devfs_unlink(struct Inode *parent, struct Inode *inode)
{
  assert(inode->dev == 1);
  (void) parent;
  return -EROFS;
}

struct Inode *
devfs_lookup(struct Inode *inode, const char *name)
{
  struct DevfsNode *device;
  
  if (inode->ino != 2)
    return NULL;

  for (device = devices; device < &devices[NDEV]; device++)
    if (strcmp(name, device->name) == 0)
      return devfs_inode_get(inode->fs, device->ino);

  return NULL;
}

void
devfs_trunc(struct Inode *inode, off_t size)
{
  (void) inode;
  (void) size;
}

struct FSOps devfs_ops = {
  .inode_read   = devfs_inode_read,
  .inode_write  = devfs_inode_write,
  .inode_delete = devfs_inode_delete,
  .read         = devfs_read,
  .write        = devfs_write,
  .trunc        = devfs_trunc,
  .rmdir        = devfs_rmdir,
  .readdir      = devfs_readdir,
  .readlink     = devfs_readlink,
  .create       = devfs_create,
  .mkdir        = devfs_mkdir,
  .mknod        = devfs_mknod,
  .link         = devfs_link,
  .unlink       = devfs_unlink,
  .lookup       = devfs_lookup,
};

int
special_open(dev_t, int, mode_t)
{
  return 0;
}

int
special_ioctl(dev_t, int, int)
{
  return 0;
}

ssize_t
special_read(dev_t dev, uintptr_t, size_t)
{
  switch (dev & 0xFF) {
  case 3:
    return 0;
  default:
    return -ENOSYS;
  }
}

ssize_t
special_write(dev_t dev, uintptr_t, size_t)
{
  switch (dev & 0xFF) {
  case 3:
    return 0;
  default:
    return -ENOSYS;
  }
}

int
special_select(dev_t, struct timeval *)
{
  return -ENOSYS;
}

struct CharDev special_device = {
  .open   = special_open,
  .ioctl  = special_ioctl,
  .read   = special_read,
  .write  = special_write,
  .select = special_select,
};

struct Inode *
devfs_mount(dev_t dev)
{
  struct FS *devfs;

  if ((devfs = (struct FS *) k_malloc(sizeof(struct FS))) == NULL)
    panic("cannot allocate FS");
  
  devfs->name  = "devfs";
  devfs->dev   = dev;
  devfs->extra = NULL;
  devfs->ops   = &devfs_ops;

  dev_register_char(0x02, &special_device);

  return devfs_inode_get(devfs, 2);
}
