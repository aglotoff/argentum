#ifndef __KERNEL_KMI_H__
#define __KERNEL_KMI_H__

/**
 * @file kernel/kmi.h
 * 
 * Keyboard input code.
 */

/** Key code for Ctrl+x */
#define C(x) ((x) - '@')

void kmi_kbd_init(void);
void kmi_kbd_intr(void);

#endif  // !__KERNEL_KMI_H__
