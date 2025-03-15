#ifndef _ARCH_I386_IOAPIC_H
#define _ARCH_I386_IOAPIC_H

void ioapic_init(void);
void ioapic_enable(int, int);
void ioapic_mask(int);
void ioapic_unmask(int);

#endif  // !_ARCH_I386_IOAPIC_H
