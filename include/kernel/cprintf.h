#ifndef __KERNEL_CPRINTF_H__
#define __KERNEL_CPRINTF_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/cprintf.h
 * 
 * Formatted output to console.
 */

#include <stdarg.h>

void vcprintf(const char *, va_list);
void cprintf(const char *, ...);

#endif  // !__KERNEL_CPRINTF_H__
