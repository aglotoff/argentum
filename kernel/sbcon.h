#ifndef __KERNEL_SBCON_H__
#define __KERNEL_SBCON_H__

/**
 * @file kernel/sbcon.h
 * 
 * Two-wire serial bus interface (SBCon).
 */

#include <time.h>

void   sb_init(void);
time_t sb_rtc_time(void);

#endif  // !__KERNEL_SBCON_H__
