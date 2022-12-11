#ifndef __KERNEL_DRIVERS_CONSOLE_PL111_H__
#define __KERNEL_DRIVERS_CONSOLE_PL111_H__

/**
 * @file kernel/drivers/console/pl111.h
 * 
 * PrimeCell Color LCD Controller (PL111) driver.
 */

#include <stdint.h>

#include <argentum/mm/memlayout.h>

/**
 * PL111 Driver instance.
 */
struct Pl111 {
  volatile uint32_t *base;      ///< Memory base address
};

/**
 * Available display resolutions.
 */
enum Pl111Resolution {
  PL111_RES_QVGA_PORTRAIT,    ///< QVGA(240x320) (portrait) on VGA
  PL111_RES_QVGA_LANDSCAPE,   ///< QVGA(240x320) (landscape) on VGA
  PL111_RES_QCIF_PORTRAIT,    ///< QCIF (176x220) (portrait) on VGA
  PL111_RES_VGA,              ///< VGA (640x480) on VGA
  PL111_RES_SVGA,             ///< SVGA (800x600) on SVGA
  PL111_RES_EPSON,            ///< Epson 2.2in panel QCIF (176x220)
  PL111_RES_SANYO,            ///< Sanyo 3.8in panel QVGA (320x240)
  PL111_RES_XGA,              ///< XGA (1024x768)
};

int pl111_init(struct Pl111 *, void *, physaddr_t, enum Pl111Resolution);

#endif  // !__KERNEL_DRIVERS_CONSOLE_PL111_H__
