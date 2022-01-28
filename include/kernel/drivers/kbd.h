#ifndef __KERNEL_DRIVERS_KBD_H__
#define __KERNEL_DRIVERS_KBD_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/kbd.h
 * 
 * Keyboard input code.
 */

/** Key code for Ctrl+x */
#define C(x) ((x) - '@')

void kbd_init(void);
void kbd_intr(void);

#endif  // !__KERNEL_DRIVERS_KBD_H__
