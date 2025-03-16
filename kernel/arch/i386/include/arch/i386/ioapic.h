#ifndef _ARCH_I386_IOAPIC_H
#define _ARCH_I386_IOAPIC_H

#include <stdint.h>

void ioapic_init(void);
void ioapic_enable(int, int);
void ioapic_mask(int);
void ioapic_unmask(int);

extern uint32_t ioapic_pa;

#endif  // !_ARCH_I386_IOAPIC_H
