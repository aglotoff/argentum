#ifndef __KERNEL_DRIVERS_CONSOLE_DISPLAY_H__
#define __KERNEL_DRIVERS_CONSOLE_DISPLAY_H__

/**
 * @file kernel/drivers/console/display.h
 *
 * Display driver.
 */

#define DEFAULT_FB_WIDTH    640
#define DEFAULT_FB_HEIGHT   480

int  display_init(void);
void display_update(struct Console *);
void display_erase(struct Console *, unsigned from, unsigned to);
void display_draw_char_at(struct Console *, unsigned);
void display_scroll_down(struct Console *, unsigned);
void display_flush(struct Console *);
void display_update_cursor(struct Console *);

#endif  // !__KERNEL_DRIVERS_CONSOLE_DISPLAY_H__
