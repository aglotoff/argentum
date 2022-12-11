#ifndef __INCLUDE_ARGENTUM_DRIVERS_ETH_H__
#define __INCLUDE_ARGENTUM_DRIVERS_ETH_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/argentum/drivers/eth.h
 * 
 * Ethernet driver.
 */

#include <stddef.h>

void eth_init(void);
void eth_intr(void);
void eth_write(const void *, size_t);

#endif  // !__INCLUDE_ARGENTUM_DRIVERS_ETH_H__
