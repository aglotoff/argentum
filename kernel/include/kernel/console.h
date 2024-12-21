#ifndef __KERNEL_INCLUDE_KERNEL_CPRINTF_H__
#define __KERNEL_INCLUDE_KERNEL_CPRINTF_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/console.h
 *
 * Formatted output to console.
 */

#include <stdarg.h>

int  arch_console_getc(void);
void arch_console_putc(char);

void console_putc(char);
int  console_getc(void);
void vcprintf(const char *, va_list);
void cprintf(const char *, ...);

#endif // !__KERNEL_INCLUDE_KERNEL_CPRINTF_H__
