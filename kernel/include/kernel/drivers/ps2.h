#ifndef __KERNEL_DRIVERS_PS2_H__
#define __KERNEL_DRIVERS_PS2_H__

#include <kernel/trap.h>

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct Pl050;

struct PS2 {
  struct Pl050 *kmi;
  struct ISRThread kbd_isr;
  int irq;
};

int ps2_init(struct PS2 *, struct Pl050 *, int);
int ps2_kbd_getc(struct PS2 *);

#endif  // !__KERNEL_DRIVERS_PS2_H__
