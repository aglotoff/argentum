#ifndef _ARCH_I386_I8259_H
#define _ARCH_I386_I8259_H

#include <stdint.h>

void i8259_init(uint8_t, int);
void i8259_mask(int);
void i8259_unmask(int);
void i8259_eoi(int);

#endif  // !_ARCH_I386_I8259_H
