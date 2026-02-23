#ifndef __KERNEL_INCLUDE_KERNEL_BOOT_ALLOC_H__
#define __KERNEL_INCLUDE_KERNEL_BOOT_ALLOC_H__

#include <stddef.h>

void boot_mem_init(void *, void *);
void boot_mem_fini(void);
void *boot_mem_alloc(size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_BOOT_ALLOC_H__
