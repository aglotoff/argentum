#ifndef __KERNEL_KBD_H__
#define __KERNEL_KBD_H__

/**
 * @file kernel/kbd.h
 * 
 * Keyboard input code.
 */

/** Key code for Ctrl+x */
#define C(x) ((x) - '@')

void kbd_init(void);
void kbd_intr(void);

#endif  // !__KERNEL_KBD_H__
