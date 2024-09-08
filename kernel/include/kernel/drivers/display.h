#ifndef __KERNEL_DRIVERS_CONSOLE_DISPLAY_H__
#define __KERNEL_DRIVERS_CONSOLE_DISPLAY_H__

/**
 * @file kernel/drivers/console/display.h
 *
 * Display driver.
 */

#include <stdint.h>

struct Screen;

struct Display {
  uint16_t     *fb_base;
  unsigned      fb_width;
  unsigned      fb_height;

  unsigned      pos;
  unsigned      cursor_pos;
  int           cursor_visible;

  struct {
    uint8_t *bitmap;
    uint8_t  glyph_width;
    uint8_t  glyph_height;
  } font;
};

#define DEFAULT_FB_WIDTH    640
#define DEFAULT_FB_HEIGHT   480

int  display_init(struct Display *, void *);
void display_update(struct Display *, struct Screen *);
void display_erase(struct Display *, struct Screen *, unsigned, unsigned);
void display_draw_char_at(struct Display *, struct Screen *, unsigned);
void display_scroll_down(struct Display *, struct Screen *, unsigned);
void display_flush(struct Display *, struct Screen *);
void display_update_cursor(struct Display *, struct Screen *);

#endif  // !__KERNEL_DRIVERS_CONSOLE_DISPLAY_H__
