#ifndef __KERNEL_INCLUDE_KERNEL_INTERRUPT_H__
#define __KERNEL_INCLUDE_KERNEL_INTERRUPT_H__

#include <kernel/core/semaphore.h>

struct KTask;
struct TrapFrame;

void arch_interrupt_init(void);
void arch_interrupt_init_percpu(void);
void arch_interrupt_ipi(void);
void arch_interrupt_mask(int);
void arch_interrupt_unmask(int);
void arch_interrupt_enable(int, int);
int  arch_interrupt_id(struct TrapFrame *);
void arch_interrupt_eoi(int);

typedef int (*interrupt_handler_t)(int, void *);

void interrupt_attach(int, interrupt_handler_t, void *);
void interrupt_attach_task(int, interrupt_handler_t, void *);
void interrupt_dispatch(struct TrapFrame *);

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
