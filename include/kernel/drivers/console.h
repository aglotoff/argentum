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

// ANSI color codes
#define COLOR_MASK            7
#define COLOR_BLACK           0
#define COLOR_RED             1
#define COLOR_GREEN           2
#define COLOR_YELLOW          3
#define COLOR_BLUE            4
#define COLOR_MAGENTA         5
#define COLOR_CYAN            6
#define COLOR_WHITE           7
#define COLOR_BRIGHT          (COLOR_MASK + 1)
#define COLOR_GRAY            (COLOR_BRIGHT + COLOR_BLACK)
#define COLOR_BRIGHT_RED      (COLOR_BRIGHT + COLOR_RED)
#define COLOR_BRIGHT_GREEN    (COLOR_BRIGHT + COLOR_GREEN)
#define COLOR_BRIGHT_YELLOW   (COLOR_BRIGHT + COLOR_YELLOW)
#define COLOR_BRIGHT_BLUE     (COLOR_BRIGHT + COLOR_BLUE)
#define COLOR_BRIGHT_MAGENTA  (COLOR_BRIGHT + COLOR_MAGENTA)
#define COLOR_BRIGHT_CYAN     (COLOR_BRIGHT + COLOR_CYAN)
#define COLOR_BRIGHT_WHITE    (COLOR_BRIGHT + COLOR_WHITE)

// Text buffer dimensions, in characters
#define BUF_WIDTH         80
#define BUF_HEIGHT        30
#define BUF_SIZE          (BUF_WIDTH * BUF_HEIGHT) 

void    console_init(void);
void    console_putc(char);
void    console_intr(int (*)(void));
int     console_getc(void);

ssize_t console_read(void *, size_t);
ssize_t console_write(const void *, size_t);

#endif  // !__KERNEL_DRIVERS_CONSOLE_H__
