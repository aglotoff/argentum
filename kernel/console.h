#ifndef __KERNEL_CONSOLE_H__
#define __KERNEL_CONSOLE_H__

/**
 * @file kernel/console.h
 * 
 * General device-independent console code.
 */

#include <stdarg.h>

/**
 * Initialize the console devices.
 */
void console_init(void);

/**
 * Output a character to the console.
 * 
 * @param c The chaacter to be written.
 */
void console_putc(char c);

/**
 * Handle console interrupt.
 * 
 * This function should be called by driver interrupt routines to feed input
 * characters into the console buffer.
 * 
 * @param getc The function to fetch the next input character. Should return -1
 *             if there is no data avalable.
 */
void console_intr(int (*getc)(void));

/**
 * Return the next input character from the console. Polls for any pending
 * input characters.
 * 
 * @returns The next input character.
 */
int  console_getc(void);

/**
 * Printf-like formatted output to the console.
 * 
 * @param format The format string.
 * @param ap     A variable argument list.
 */
void vcprintf(const char *format, va_list ap);

/**
 * Printf-like formatted output to the console.
 * 
 * @param format The format string.
 */
void cprintf(const char *format, ...);

#endif  // !__KERNEL_CONSOLE_H__
