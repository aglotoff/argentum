#ifndef __INCLUDE_ARGENTUM_IRQ_H__
#define __INCLUDE_ARGENTUM_IRQ_H__

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
void         irq_disable(void);
void         irq_enable(void);
void         irq_save(void);
void         irq_restore(void);
void         irq_dispatch(void);
int          irq_attach(int, int (*)(void), int);

#endif  // !__INCLUDE_ARGENTUM_IRQ_H__
