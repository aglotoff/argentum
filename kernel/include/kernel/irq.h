#ifndef __KERNEL_INCLUDE_KERNEL_IRQ_H__
#define __KERNEL_INCLUDE_KERNEL_IRQ_H__

// IRQ numbers
#define IRQ_PTIMER    29
#define IRQ_UART0     44
#define IRQ_MCIA      49
#define IRQ_MCIB      50
#define IRQ_KMI0      52
#define IRQ_ETH       60
#define IRQ_MAX       64

void         irq_init(void);
void         irq_init_percpu(void);
void         irq_dispatch(void);
int          irq_attach(int, void (*)(void), int);

#endif  // !__KERNEL_INCLUDE_KERNEL_IRQ_H__
