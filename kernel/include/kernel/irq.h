#ifndef __KERNEL_INCLUDE_KERNEL_IRQ_H__
#define __KERNEL_INCLUDE_KERNEL_IRQ_H__

int             k_arch_irq_is_enabled(void);
void            k_arch_irq_enable(void);
void            k_arch_irq_disable(void);
int             k_arch_irq_save(void);
void            k_arch_irq_restore(int);

typedef int (*k_irq_handler_t)(void *);

void k_irq_attach(int, k_irq_handler_t, void *);
void k_irq_save(void);
void k_irq_restore(void);
void k_irq_begin(void);
void k_irq_end(void);
int  k_irq_dispatch(int);

/**
 * Disable all interrupts on the local (current) processor core.
 */
static inline void
k_irq_disable(void)
{
  k_arch_irq_disable();
}

/**
 * Enable all interrupts on the local (current) processor core.
 */
static inline void
k_irq_enable(void)
{
  k_arch_irq_enable();
}

#endif  // !__KERNEL_INCLUDE_KERNEL_IRQ_H__
