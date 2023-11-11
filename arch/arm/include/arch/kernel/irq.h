#ifndef __AG_INCLUDE_ARCH_KERNEL_IRQ_H__
#define __AG_INCLUDE_ARCH_KERNEL_IRQ_H__

#define ARCH_IRQ_MAX  64

#ifndef __ASSEMBLER__

void arch_irq_init(void);
void arch_irq_init_percpu(void);
int  arch_irq_is_enabled(void);
void arch_irq_disable(void);
void arch_irq_enable(void);
int  arch_irq_save(void);
void arch_irq_restore(int);
void arch_irq_unmask(int);
void arch_irq_mask(int);
void arch_irq_dispatch(void);
void arch_irq_ipi_single(int, int);
void arch_irq_ipi_self(int);
void arch_irq_ipi_others(int);

#endif  // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_ARCH_KERNEL_IRQ_H__
