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
    unsigned            cursor_pos;                   // Current cursor position
    int                 cursor_visible;
    int                 fg_color;                     // Current foreground color
    int                 bg_color;                     // Current background color
    enum ParserState    state;          
    int                 esc_params[CONSOLE_ESC_MAX];  // The esc sequence parameters
    int                 esc_cur_param;                // Index of the current esc parameter
    struct {
      unsigned ch : 8;
      unsigned fg : 4;
      unsigned bg : 4;
    }                   buf[CONSOLE_COLS * CONSOLE_ROWS];
    unsigned            cols;
    unsigned            rows;
    unsigned            pos;
  } out;
};

extern struct Console console_current;

void    console_init(void);
void    console_putc(char);
void    console_interrupt(char *);
int     console_getc(void);
ssize_t console_read(uintptr_t, size_t);
ssize_t console_write(const void *, size_t);
int     console_ioctl(int, int);

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__
