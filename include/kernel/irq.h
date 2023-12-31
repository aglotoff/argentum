#ifndef __AG_INCLUDE_KERNEL_IRQ_H__
#define __AG_INCLUDE_KERNEL_IRQ_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <arch/kernel/irq.h>
#include <kernel/list.h>

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
void irq_handler_enter(void);
void irq_handler_exit(void);

#endif  // !__AG_INCLUDE_KERNEL_IRQ_H__
