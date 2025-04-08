#ifndef __KERNEL_FS_DEV_H__
#define __KERNEL_FS_DEV_H__

#include <stdint.h>
#include <sys/types.h>

#include <kernel/fs/fs.h>

ino_t devfs_mount(dev_t, struct FS **);

#endif  // !__KERNEL_FS_DEV_H__
