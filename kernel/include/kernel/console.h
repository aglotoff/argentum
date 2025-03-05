#ifndef __KERNEL_INCLUDE_KERNEL_CONSOLE_H__
#define __KERNEL_INCLUDE_KERNEL_CONSOLE_H__

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

void _panic(const char *, int, const char *, ...);
void _warn(const char *, int, const char *, ...);

#endif // !__KERNEL_INCLUDE_KERNEL_CONSOLE_H__
