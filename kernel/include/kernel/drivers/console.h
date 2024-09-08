#ifndef __KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__
#define __KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/drivers/console.h
 * 
 * General device-independent console code.
 */

#include <stdarg.h>
#include <sys/types.h>
#include <sys/termios.h>

#include <kernel/spinlock.h>
#include <kernel/waitqueue.h>

#define TTY_INPUT_MAX     256
#define SCREEN_ESC_MAX    16   // The maximum number of esc parameters
#define SCREEN_COLS       80
#define SCREEN_ROWS       30

enum ParserState {
  PARSER_NORMAL,
  PARSER_ESC,
  PARSER_CSI,
};

struct Screen {
  int                 fg_color;                     // Current foreground color
  int                 bg_color;                     // Current background color
  enum ParserState    state;          
  unsigned            esc_params[SCREEN_ESC_MAX];  // The esc sequence parameters
  int                 esc_cur_param;                // Index of the current esc parameter
  int                 esc_question;
  struct {
    unsigned ch : 8;
    unsigned fg : 4;
    unsigned bg : 4;
  }                   buf[SCREEN_COLS * SCREEN_ROWS];
  unsigned            cols;
  unsigned            rows;
  unsigned            pos;
  int                 stopped;
  struct KSpinLock     lock;
};

struct Tty {
  struct {
    char                buf[TTY_INPUT_MAX];
    size_t              size;
    size_t              read_pos;
    size_t              write_pos;
    struct KSpinLock    lock;
    struct KWaitQueue   queue;
  } in;
  struct Screen       *out;
  struct termios        termios;
  pid_t                 pgrp;
};

extern struct Tty *tty_current;
extern struct Tty *tty_system;

void    tty_init(void);

void    console_putc(char);
void    tty_process_input(struct Tty *, char *);
int     console_getc(void);
ssize_t tty_read(dev_t, uintptr_t, size_t);
ssize_t tty_write(dev_t, uintptr_t, size_t);
int     tty_ioctl(dev_t, int, int);
int     tty_select(dev_t, struct timeval *);
void    tty_switch(int);

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

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__
