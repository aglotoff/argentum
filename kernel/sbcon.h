#ifndef __KERNEL_SBCON_H__
#define __KERNEL_SBCON_H__

/**
 * @file kernel/sbcon.h
 * 
 * Two-wire serial bus interface (SBCon).
 */

void sb_init(void);
void sb_rtc_time(void);

#endif  // !__KERNEL_SBCON_H__
