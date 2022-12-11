#ifndef __KERNEL_DRIVERS_CONSOLE_H__
#define __KERNEL_DRIVERS_CONSOLE_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/console.h
 * 
 * General device-independent console code.
 */

#include <stdarg.h>
#include <sys/types.h>

void    console_init(void);
void    console_putc(char);
void    console_interrupt(int (*)(void));
int     console_getc(void);
ssize_t console_read(void *, size_t);
ssize_t console_write(const void *, size_t);

#endif  // !__KERNEL_DRIVERS_CONSOLE_H__
