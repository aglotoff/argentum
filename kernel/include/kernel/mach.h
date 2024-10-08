#ifndef __KERNEL_INCLUDE_KERNEL_MACH_H__
#define __KERNEL_INCLUDE_KERNEL_MACH_H__

#include <stdint.h>
#include <time.h>

#define MACH_REALVIEW_PB_A8   1897
#define MACH_REALVIEW_PBX_A9  1901

#define MACH_MAX  5108

struct Buf;
struct Screen;
struct Tty;

struct Machine {
  uint32_t type;

  void   (*interrupt_ipi)(void);
  int    (*interrupt_id)(void);
  void   (*interrupt_enable)(int, int);
  void   (*interrupt_mask)(int);
  void   (*interrupt_unmask)(int);
  void   (*interrupt_init)(void);
  void   (*interrupt_init_percpu)(void);
  void   (*interrupt_eoi)(int);

  void   (*timer_init)(void);
  void   (*timer_init_percpu)(void);

  void   (*rtc_init)(void);
  time_t (*rtc_get_time)(void);
  void   (*rtc_set_time)(time_t);

  int    (*storage_init)(void);
 
  int    (*console_init)(void);
  int    (*console_getc)(void);
  void   (*console_putc)(char);

  void   (*tty_out_char)(struct Tty *, char);
  void   (*tty_flush)(struct Tty *);
  void   (*tty_erase)(struct Tty *);
  void   (*tty_switch)(struct Tty *);
  void   (*tty_init_system)(void);
  void   (*tty_init)(struct Tty *, int);

  int    (*eth_init)(void);
  void   (*eth_write)(const void *, size_t);
};

extern struct Machine *mach_current;

void mach_init(uint32_t);

#define MACH_DEFINE(name)  \
  __attribute__((section(".mach"))) struct Machine __mach_##name = 

#endif  // !__KERNEL_INCLUDE_KERNEL_MACH_H__
