#ifndef __KERNEL_FS_DEV_H__
#define __KERNEL_FS_DEV_H__

#include <stdint.h>
#include <sys/types.h>

#include <kernel/fs/fs.h>

struct Inode *dev_mount(dev_t);

#endif  // !__KERNEL_FS_DEV_H__
