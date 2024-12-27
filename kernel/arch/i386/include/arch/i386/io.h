#ifndef __ARCH_I386_IO_H__
#define __ARCH_I386_IO_H__

#include <stdint.h>

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static inline void
outb(int port, uint8_t data)
{
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
}

#endif  // !__ARCH_I386_IO_H__
