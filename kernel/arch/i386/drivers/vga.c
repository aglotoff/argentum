#include <string.h>

#include <arch/i386/io.h>
#include <arch/i386/vga.h>

// Map ANSI color codes to VGA colors
static uint16_t
colors[] = {
  [COLOR_BLACK]           = 0x0,
  [COLOR_RED]             = 0x4,
  [COLOR_GREEN]           = 0x2,
  [COLOR_YELLOW]          = 0x6,
  [COLOR_BLUE]            = 0x1,
  [COLOR_MAGENTA]         = 0x5,
  [COLOR_CYAN]            = 0x3,
  [COLOR_WHITE]           = 0x7,
  [COLOR_GRAY]            = 0x8,
  [COLOR_BRIGHT_RED]      = 0xc,
  [COLOR_BRIGHT_GREEN]    = 0xa,
  [COLOR_BRIGHT_YELLOW]   = 0xe,
  [COLOR_BRIGHT_BLUE]     = 0x9,
  [COLOR_BRIGHT_MAGENTA]  = 0xd,
  [COLOR_BRIGHT_CYAN]     = 0xb,
  [COLOR_BRIGHT_WHITE]    = 0xf,
};

static void
vga_move_cursor(struct Vga *vga, unsigned pos)
{
  (void) vga;

  outb(0x3D4, 14);
	outb(0x3D4 + 1, pos >> 8);
	outb(0x3D4, 15);
	outb(0x3D4 + 1, pos);
}

void
vga_init(struct Vga *vga, void *buffer, struct Screen *screen)
{ 
  vga->buffer = (uint16_t *) buffer;
  vga->color  = 0x0e00;
  vga->screen = screen;
}

static int
vga_draw_char_at(void *ctx, unsigned i)
{
  struct Vga *vga = (struct Vga *) ctx;
  uint16_t attr;
  
  attr = vga->screen->buf[i].ch;
  attr |= (colors[vga->screen->buf[i].fg] << 8);
  attr |= (colors[vga->screen->buf[i].bg] << 12);

  vga->buffer[i] = attr;

  return 0;
}

static int
vga_erase(void *ctx, unsigned from, unsigned to)
{
  struct Vga *vga = (struct Vga *) ctx;
  size_t i;

  if (from < to) {
    for (i = from; i != to; i++)
      vga->buffer[i] = (vga->buffer[i] & ~0xFF) | ' ';
  } else {
    for (i = from; i != to; i--)
      vga->buffer[i] = (vga->buffer[i] & ~0xFF) | ' ';
  }

  return 0;
}

static int
vga_scroll_down(void *ctx, unsigned n)
{
  struct Vga *vga = (struct Vga *) ctx;

  memmove(&vga->buffer[0],
          &vga->buffer[vga->screen->cols * n],
          sizeof(vga->buffer[0]) * vga->screen->cols * (vga->screen->rows - n));

  return 0;
}

static int
vga_update(void *ctx, struct Screen *screen)
{
  struct Vga *vga = (struct Vga *) ctx;
  size_t i;
  
  vga->screen = screen;

  for (i = 0; i < vga->screen->cols * vga->screen->rows; i++)
    vga_draw_char_at(vga, i);

  vga_move_cursor(vga, screen->new_pos);

  return 0;
}

static int
vga_update_cursor(void *ctx, unsigned pos)
{
  struct Vga *vga = (struct Vga *) ctx;

  vga_move_cursor(vga, pos);

  return 0;
}

struct ScreenOps vga_ops = {
  .draw_char_at = vga_draw_char_at,
  .erase = vga_erase,
  .scroll_down = vga_scroll_down,
  .update = vga_update,
  .update_cursor = vga_update_cursor
};
