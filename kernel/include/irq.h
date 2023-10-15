#ifndef __AG_KERNEL_IRQ_H__
#define __AG_KERNEL_IRQ_H__

#include <arch_irq.h>
#include <list.h>

/** Maximum number of IRQ vectors supported in the system */
#define IRQ_MAX ARCH_IRQ_MAX

struct IrqHook {
  /** Link into the hook chain */
  struct ListLink link;
  /** Pointer to the interrupt handler function */
  int           (*handler)(int);
  /** IRQ vector number */
  int             irq;
  /** Hook ID (unique within the corresponding IRQ vector) */
  int             id;
};

/**
 * Disable all interrupts on the local (current) processor core.
 */
static inline void
irq_disable(void)
{
  arch_irq_disable();
}

/**
 * Enable all interrupts on the local (current) processor core.
 */
static inline void
irq_enable(void)
{
  arch_irq_enable();
}

void irq_init(void);
void irq_init_percpu(void);
void irq_save(void);
void irq_restore(void);
int  irq_hook_attach(struct IrqHook *, int, int (*)(int));
int  irq_hook_detach(struct IrqHook *);
int  irq_hook_enable(struct IrqHook *);
int  irq_hook_disable(struct IrqHook *);
void irq_handle(int);

#endif  // !__AG_KERNEL_IRQ_H__
