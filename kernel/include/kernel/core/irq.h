#ifndef __KERNEL_INCLUDE_KERNEL_CORE_IRQ_H__
#define __KERNEL_INCLUDE_KERNEL_CORE_IRQ_H__

int  k_arch_irq_is_enabled(void);
void k_arch_irq_enable(void);
void k_arch_irq_disable(void);
int  k_arch_irq_state_save(void);
void k_arch_irq_state_restore(int);

void k_irq_state_save(void);
void k_irq_state_restore(void);
void k_irq_handler_begin(void);
void k_irq_handler_end(void);

static inline void
k_irq_disable(void)
{
  k_arch_irq_disable();
}

static inline void
k_irq_enable(void)
{
  k_arch_irq_enable();
}

#endif  // !__KERNEL_INCLUDE_KERNEL_CORE_IRQ_H__
