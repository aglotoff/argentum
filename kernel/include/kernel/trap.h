#ifndef __KERNEL_INCLUDE_KERNEL_TRAP_H__
#define __KERNEL_INCLUDE_KERNEL_TRAP_H__

#include <arch/trap.h>

int timer_irq(int, void *);
int ipi_irq(int, void *);

#endif  // __KERNEL_INCLUDE_KERNEL_TRAP_H__
