#include <kernel/assert.h>
#include <kernel/dev.h>
#include <kernel/cprintf.h>

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
    panic("bad major dev number %d", major);

  if (dev_char[major] != NULL)
    panic("character device with major %d already registered", major);

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
    panic("bad major dev number %d", major);

  if (dev_block[major] != NULL)
    panic("block device with major %d already registered", major);

  dev_block[major] = dev;
}
