#ifndef __KERNEL_DRIVERS_PS2_H__
#define __KERNEL_DRIVERS_PS2_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct PS2Ops {
  void (*putc)(void *, char);
  int  (*getc)(void *);
};

struct PS2 {
  struct PS2Ops    *ops;
  void             *ops_arg;
};

int ps2_init(struct PS2 *, struct PS2Ops *, void *, int);
int ps2_kbd_getc(struct PS2 *);

#endif  // !__KERNEL_DRIVERS_PS2_H__
