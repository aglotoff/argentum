#ifndef __KERNEL_DRIVERS_SD_PL180_H__
#define __KERNEL_DRIVERS_SD_PL180_H__

#include <stdint.h>
#include <kernel/drivers/sd.h>

struct PL180 {
  volatile uint32_t *base;
};

int  pl180_init(struct PL180 *, void *);

extern struct SDOps pl180_ops;

#endif  // !__KERNEL_DRIVERS_SD_PL180_H__
