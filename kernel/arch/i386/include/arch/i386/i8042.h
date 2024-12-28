#ifndef _ARCH_I386_I8042_H
#define _ARCH_I386_I8042_H

#include <stdint.h>
#include <kernel/drivers/ps2.h>

struct I8042 {
  struct PS2         ps2;
};

int  i8042_init(struct I8042 *, int);
int  i8042_getc(struct I8042 *);

#endif  // !_ARCH_I386_I8042_H
