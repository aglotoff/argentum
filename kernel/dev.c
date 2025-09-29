#include <kernel/core/assert.h>
#include <kernel/dev.h>
#include <kernel/console.h>
#include <kernel/ipc/channel.h>

#define DEV_MAJOR_MIN 0
#define DEV_MAJOR_MAX 255

static struct CharDev *dev_char[256];

struct CharDev *
dev_lookup_char(dev_t dev)
{
  int major = dev >> 8;
  return dev_char[major];
}

void
dev_register_char(int major, struct CharDev *dev)
{
  if ((major < DEV_MAJOR_MIN) || (major > DEV_MAJOR_MAX))
    k_panic("bad major dev number %d", major);

  if (dev_char[major] != NULL)
    k_panic("character device with major %d already registered", major);

  dev_char[major] = dev;
}

static struct BlockDev *dev_block[256];

struct BlockDev *
dev_lookup_block(dev_t dev)
{
  int major = dev >> 8;
  return dev_block[major];
}

void
dev_register_block(int major, struct BlockDev *dev)
{
  if ((major < DEV_MAJOR_MIN) || (major > DEV_MAJOR_MAX))
    k_panic("bad major dev number %d", major);

  if (dev_block[major] != NULL)
    k_panic("block device with major %d already registered", major);

  dev_block[major] = dev;
}

int
dev_open(struct IpcRequest *req, dev_t rdev, int oflag, mode_t mode)
{
  struct CharDev *d = dev_lookup_char(rdev);

  if (d == NULL)
    return -ENODEV;

  return d->open(req, rdev, oflag, mode);
}

ssize_t
dev_read(struct IpcRequest *req, dev_t rdev, uintptr_t va, size_t n)
{
  struct CharDev *d = dev_lookup_char(rdev);

  if (d == NULL)
    return -ENODEV;

  return d->read(req, rdev, va, n);
}

ssize_t
dev_write(struct IpcRequest *req, dev_t rdev, uintptr_t va, size_t n)
{
  struct CharDev *d = dev_lookup_char(rdev);

  if (d == NULL)
    return -ENODEV;

  return d->write(req, rdev, va, n);
}

int
dev_ioctl(struct IpcRequest *req, dev_t rdev, int request, int arg)
{
  struct CharDev *d = dev_lookup_char(rdev);

  if (d == NULL)
    return -ENODEV;

  return d->ioctl(req, rdev, request, arg);
}

int
dev_select(struct IpcRequest *req, dev_t rdev, struct timeval *timeout)
{
  struct CharDev *d = dev_lookup_char(rdev);

  if (d == NULL)
    return -ENODEV;

  return d->select(req, rdev, timeout);
}
