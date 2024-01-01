#ifndef __KERNEL_INCLUDE_KERNEL_MMU_H__
#define __KERNEL_INCLUDE_KERNEL_MMU_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/armv7/mmu.h>
#include <kernel/armv7/regs.h>
#include <kernel/mm/memlayout.h>

void   *vm_create(void);
void    vm_destroy(void *);







void         vm_init(void);
void         vm_init_percpu(void);

void         vm_load_kernel(void);
void         vm_load(void *);

#endif  // !__KERNEL_INCLUDE_KERNEL_MMU_H__
