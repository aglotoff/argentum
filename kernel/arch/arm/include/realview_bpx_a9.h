#ifndef __AG_KERNEL_REALVIEW_PBX_A9_H__
#define __AG_KERNEL_REALVIEW_PBX_A9_H__

/**
 * @file kernel/realview_pbx_a9.h
 * 
 * Definitions related to the Realview PBX-A9 board
 */

/** Flag Register */
#define SYS_FLAGS         0x10000030
/** Flag Set Register */
#define SYS_FLAGSSET      0x10000030

/** UART 0 Interface physical base address */
#define UART0_BASE        0x10009000
/** UART clock rate, in Hz */
#define UART_CLOCK        24000000U

/** Private Memory Region physical base address */
#define PRIV_BASE         0x1F000000
/** Interrupt Controller physical base address */
#define GICC_BASE         (PRIV_BASE + 0x0100)
/** Interrupt Distributor physical base address */
#define GICD_BASE         (PRIV_BASE + 0x1000)
/** Private Timer physical base address */
#define MPTIMER_BASE      (PRIV_BASE + 0x0600)
/** Private Timer IRQ */
#define MPTIMER_IRQ       29

#endif  // !__AG_KERNEL_REALVIEW_PBX_A9_H__
