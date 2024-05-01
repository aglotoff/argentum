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

#include <kernel/spin.h>
#include <kernel/wchan.h>

#define CONSOLE_INPUT_MAX 256
#define CONSOLE_ESC_MAX   16   // The maximum number of esc parameters
#define CONSOLE_COLS   80
#define CONSOLE_ROWS  30

enum ParserState {
  PARSER_NORMAL,
  PARSER_ESC,
  PARSER_CSI,
};

struct Console {
  struct {
    char                buf[CONSOLE_INPUT_MAX];
    size_t              size;
    size_t              read_pos;
    size_t              write_pos;
    struct SpinLock     lock;
    struct WaitChannel  queue;
  } in;
  struct {
    int                 fg_color;                     // Current foreground color
    int                 bg_color;                     // Current background color
    enum ParserState    state;          
    unsigned            esc_params[CONSOLE_ESC_MAX];  // The esc sequence parameters
    int                 esc_cur_param;                // Index of the current esc parameter
    int                 esc_question;
    struct {
      unsigned ch : 8;
      unsigned fg : 4;
      unsigned bg : 4;
    }                   buf[CONSOLE_COLS * CONSOLE_ROWS];
    unsigned            cols;
    unsigned            rows;
    unsigned            pos;
    int                 stopped;
    struct SpinLock     lock;
  } out;
  struct termios        termios;
  pid_t                 pgrp;
};

extern struct Console *console_current;

struct Inode;

void    console_init(void);
void    console_putc(char);
void    console_interrupt(char *);
int     console_getc(void);
ssize_t console_read(struct Inode *, uintptr_t, size_t);
ssize_t console_write(struct Inode *, const void *, size_t);
int     console_ioctl(struct Inode *, int, int);
void    console_switch(int);

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
