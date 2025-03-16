#ifndef _ARCH_I386_LAPIC_H
#define _ARCH_I386_LAPIC_H

#include <stddef.h>
#include <stdint.h>

void     lapic_init(void);
void     lapic_init_percpu(void);
void     lapic_eoi(void);
unsigned lapic_id(void);
void     lapic_start(unsigned, uintptr_t);

extern uint32_t lapic_pa;
extern size_t   lapic_ncpus;

#endif  // !_ARCH_I386_LAPIC_H
