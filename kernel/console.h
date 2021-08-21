#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stdarg.h>

/**
 * Output a character to the console.
 * 
 * @param c The chaacter to be written.
 */
void console_putc(char c);

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

#endif  // !_CONSOLE_H_
