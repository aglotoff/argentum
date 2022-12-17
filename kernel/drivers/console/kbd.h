#ifndef __KERNEL_DRIVERS_CONSOLE_KBD_H__
#define __KERNEL_DRIVERS_CONSOLE_KBD_H__

/**
 * @file kernel/drivers/console/kbd.h
 * 
 * PS/2 Keyboard driver.
 */

/** Key code for Ctrl+x */
#define C(x) ((x) - '@')

void kbd_init(void);
int  kbd_getc(void);

#endif  // !__KERNEL_DRIVERS_CONSOLE_KBD_H__
