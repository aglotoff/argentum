#ifndef __KERNEL_DRIVERS_ETH_H__
#define __KERNEL_DRIVERS_ETH_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/eth.h
 * 
 * Ethernet driver.
 */

#include <stddef.h>

void eth_init(void);
void eth_intr(void);
void eth_write(const void *, size_t);

#endif  // !__KERNEL_DRIVERS_ETH_H__
