#ifndef __KERNEL_INCLUDE_KERNEL_DRIVERS_ETH_H__
#define __KERNEL_INCLUDE_KERNEL_DRIVERS_ETH_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/drivers/eth.h
 * 
 * Ethernet driver.
 */

#include <stddef.h>

void eth_init(void);
void eth_write(const void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_ETH_H__
