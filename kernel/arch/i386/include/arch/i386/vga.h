#ifndef _ARCH_I386_VGA_H
#define _ARCH_I386_VGA_H

#include <stddef.h>
#include <stdint.h>

#include <kernel/drivers/screen.h>

struct Vga {
  uint16_t      *buffer;
  size_t         pos;
  uint16_t       color;
  struct Screen *screen;
};

void vga_init(struct Vga *, void *, struct Screen *screen);

extern struct ScreenOps vga_ops;

#endif  // !_ARCH_I386_VGA_H
