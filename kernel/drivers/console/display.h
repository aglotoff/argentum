#ifndef __KERNEL_DRIVERS_CONSOLE_DISPLAY_H__
#define __KERNEL_DRIVERS_CONSOLE_DISPLAY_H__

/**
 * @file kernel/drivers/console/display.h
 *
 * Display driver.
 */

int  display_init(void);
void display_putc(char);
void display_flush(void);

#endif  // !__KERNEL_DRIVERS_CONSOLE_DISPLAY_H__