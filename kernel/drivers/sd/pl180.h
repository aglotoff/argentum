#ifndef __KERNEL_DRIVERS_SD_PL180_H__
#define __KERNEL_DRIVERS_SD_PL180_H__

#include <stddef.h>
#include <stdint.h>

struct PL180 {
  volatile uint32_t *base;
};

int  pl180_init(struct PL180 *, void *);
void pl180_intr_enable(struct PL180 *);
int  pl180_send_command(struct PL180 *, uint32_t, uint32_t, int, uint32_t *);
void pl180_start(struct PL180 *, uint32_t, int);
int  pl180_read_data(struct PL180 *, void *, size_t);
int  pl180_write_data(struct PL180 *, const void *, size_t);

#endif  // !__KERNEL_DRIVERS_SD_PL180_H__
