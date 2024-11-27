#ifndef __KERNEL_INCLUDE_KERNEL_INTERRUPT_H__
#define __KERNEL_INCLUDE_KERNEL_INTERRUPT_H__

#include <kernel/core/semaphore.h>

struct KThread;

void arch_interrupt_init(void);
void arch_interrupt_init_percpu(void);
void arch_interrupt_ipi(void);
void arch_interrupt_mask(int);
void arch_interrupt_unmask(int);
void arch_interrupt_enable(int, int);
int  arch_interrupt_id(void);
void arch_interrupt_eoi(int);

typedef int (*interrupt_handler_t)(int, void *);

void interrupt_attach(int, interrupt_handler_t, void *);
void interrupt_attach_thread(int, interrupt_handler_t, void *);
void interrupt_dispatch(void);

static inline void
interrupt_mask(int irq)
{
  arch_interrupt_mask(irq);
}

static inline void
interrupt_unmask(int irq)
{
  arch_interrupt_unmask(irq);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_INTERRUPT_H__
