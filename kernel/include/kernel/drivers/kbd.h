#ifndef __KERNEL_DRIVERS_CONSOLE_KBD_H__
#define __KERNEL_DRIVERS_CONSOLE_KBD_H__

/**
 * @file kernel/drivers/console/kbd.h
 * 
 * PS/2 Keyboard driver.
 */

/** Key code for Ctrl+x */
#define C(x) ((x) - '@')

#define KEY_MAX             512

// Special key codes
#define KEY_EXT             0x100
#define KEY_UP              (KEY_EXT + 0x1)
#define KEY_DOWN            (KEY_EXT + 0x2)
#define KEY_RIGHT           (KEY_EXT + 0x3)
#define KEY_LEFT            (KEY_EXT + 0x4)
#define KEY_HOME            (KEY_EXT + 0x5)
#define KEY_INSERT          (KEY_EXT + 0x6)
#define KEY_BTAB            (KEY_EXT + 0x7)

#endif  // !__KERNEL_DRIVERS_CONSOLE_KBD_H__
