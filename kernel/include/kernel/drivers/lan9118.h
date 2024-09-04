#ifndef __KERNEL_INCLUDE_KERNEL_DRIVERS_ETH_H__
#define __KERNEL_INCLUDE_KERNEL_DRIVERS_ETH_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/drivers/lan9118.h
 * 
 * Ethernet driver.
 */

#include <stddef.h>
#include <stdint.h>

struct Lan9118 {
  volatile uint32_t *base;
};

void lan9118_init(struct Lan9118 *);
void lan9118_write(struct Lan9118 *, const void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_ETH_H__
