#ifndef __KERNEL_DRIVERS_FRAMEBUFFER_H__
#define __KERNEL_DRIVERS_FRAMEBUFFER_H__

#include <stdint.h>
#include <kernel/drivers/screen.h>

struct FrameBuffer {
  uint16_t     *base;
  unsigned      width;
  unsigned      height;

  unsigned      cursor_pos;
  int           cursor_visible;

  struct {
    uint8_t *bitmap;
    uint8_t  glyph_width;
    uint8_t  glyph_height;
  } font;

  struct Screen *screen;
};

int  framebuffer_init(struct FrameBuffer *, void *, unsigned, unsigned);

extern struct ScreenOps framebuffer_ops;

#endif  // !__KERNEL_DRIVERS_FRAMEBUFFER_H__
