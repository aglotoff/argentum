#ifndef __KERNEL_INCLUDE_KERNEL_MACH_H__
#define __KERNEL_INCLUDE_KERNEL_MACH_H__

#include <stdint.h>

#define MACH_REALVIEW_PB_A8   1897
#define MACH_REALVIEW_PBX_A9  1901

#define MACH_MAX  5108

extern int mach_type;

struct Machine {
  void (*interrupt_ipi)(void);
  int (*irq_id)(void);
  void (*interrupt_enable)(int, int);
  void (*interrupt_mask)(int);
  void (*interrupt_unmask)(int);
  void (*interrupt_init)(void);
  void (*interrupt_init_percpu)(void);
  void (*interrupt_eoi)(int);

  void (*timer_init)(void);
  void (*timer_init_percpu)(void);
};

extern struct Machine *mach[];

#endif  // !__KERNEL_INCLUDE_KERNEL_MACH_H__
