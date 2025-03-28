#ifndef __KERNEL_INCLUDE_KERNEL_TRAP_H__
#define __KERNEL_INCLUDE_KERNEL_TRAP_H__

#include <arch/trap.h>

struct Process;

int  arch_trap_frame_init(struct Process *, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void arch_trap_frame_pop(struct TrapFrame *);
int  arch_trap_is_user(struct TrapFrame *);

int timer_irq(int, void *);
int ipi_irq(int, void *);

#endif  // __KERNEL_INCLUDE_KERNEL_TRAP_H__
