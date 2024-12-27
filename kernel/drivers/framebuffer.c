#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/types.h>

#include <kernel/drivers/framebuffer.h>

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be equal to PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

static void framebuffer_cursor_show(struct FrameBuffer *);
static void framebuffer_cursor_hide(struct FrameBuffer *);
static void framebuffer_draw_char(struct FrameBuffer *, unsigned, char, uint16_t, uint16_t);

static int
framebuffer_draw_char_at(void *ctx, unsigned i)
{
  struct FrameBuffer *fb = (struct FrameBuffer *) ctx;

  framebuffer_draw_char(fb,
                        i,
                        fb->screen->buf[i].ch,
                        fb->screen->buf[i].fg,
                        fb->screen->buf[i].bg);

  if (i == fb->cursor_pos)
    fb->cursor_visible = 0;

  return 0;
}

int
framebuffer_init(struct FrameBuffer *fb,
                 void *base,
                 unsigned width,
                 unsigned height)
{
  extern uint8_t _binary_kernel_drivers_console_vga_font_psf_start[];

  struct PsfHeader *psf;

  psf = (struct PsfHeader *) _binary_kernel_drivers_console_vga_font_psf_start;
  if ((psf->magic != PSF_MAGIC) || (psf->charsize != 16))
    return -EINVAL;

  fb->font.bitmap       = (uint8_t *) (psf + 1);
  fb->font.glyph_width  = 8;
  fb->font.glyph_height = psf->charsize;

  fb->cursor_pos = 0;
  fb->cursor_visible = 0;

  fb->width  = width;
  fb->height = height;
  fb->base   = (uint16_t *) base;

  return 0;
}

static int
framebuffer_update(void *ctx, struct Screen *screen)
{
  struct FrameBuffer *fb = (struct FrameBuffer *) ctx;
  unsigned i;

  fb->screen = screen;

  for (i = 0; i < screen->cols * screen->rows; i++)
    framebuffer_draw_char_at(fb, i);

  fb->cursor_pos = screen->new_pos;

  framebuffer_cursor_show(fb);

  return 0;
}

static int
framebuffer_update_cursor(void *ctx, unsigned pos)
{
  struct FrameBuffer *fb = (struct FrameBuffer *) ctx;

  if (fb->cursor_pos != pos) {
    framebuffer_cursor_hide(fb);
    fb->cursor_pos = pos;
  }

  framebuffer_cursor_show(fb);

  return 0;
}

static int
framebuffer_erase(void *ctx, unsigned from, unsigned to)
{
  struct FrameBuffer *fb = (struct FrameBuffer *) ctx;
  unsigned i;
  
  for (i = from; i <= to; i++) {
    framebuffer_draw_char(fb,
                          i,
                          ' ',
                          fb->screen->buf[i].fg, fb->screen->buf[i].bg);

    if (i == fb->cursor_pos)
      fb->cursor_visible = 0;
  }

  return 0;
}

static int
framebuffer_scroll_down(void *ctx, unsigned n)
{
  struct FrameBuffer *fb = (struct FrameBuffer *) ctx;

  if (fb->cursor_pos < fb->screen->cols * n) {
    fb->cursor_pos = 0;
    fb->cursor_visible = 0;
  } else {
    fb->cursor_pos -= fb->screen->cols * n;
  }

  memmove(&fb->base[0],
          &fb->base[fb->width * fb->font.glyph_height * n],
          sizeof(fb->base[0]) * fb->width * (fb->height - fb->font.glyph_height * n));

  return 0;
}

struct ScreenOps
framebuffer_ops = {
  .update        = framebuffer_update,
  .draw_char_at  = framebuffer_draw_char_at,
  .update_cursor = framebuffer_update_cursor,
  .erase         = framebuffer_erase,
  .scroll_down   = framebuffer_scroll_down,
};

static void
framebuffer_cursor_hide(struct FrameBuffer *fb)
{
  if (fb->cursor_visible) {
    framebuffer_draw_char(fb,
                          fb->cursor_pos,
                          fb->screen->buf[fb->cursor_pos].ch,
                          fb->screen->buf[fb->cursor_pos].fg,
                          fb->screen->buf[fb->cursor_pos].bg);
    fb->cursor_visible = 0;
  }
}

static void
framebuffer_cursor_show(struct FrameBuffer *fb)
{
  if (!fb->cursor_visible) {
    framebuffer_draw_char(fb, 
                          fb->cursor_pos,
                          fb->screen->buf[fb->cursor_pos].ch,
                          fb->screen->buf[fb->cursor_pos].bg,
                          fb->screen->buf[fb->cursor_pos].fg);
    fb->cursor_visible = 1;
  }
}

// Create a 16-bit 5:6:5 RGB color reperesentation
#define RGB565(r, g, b)   (((r) / 8) | (((g) / 4) << 5) | (((b) / 8) << 11))

// Map ANSI color codes to 16-bit RGB colors
static uint16_t
colors[] = {
  [COLOR_BLACK]           = RGB565(0, 0, 0),
  [COLOR_RED]             = RGB565(222, 56, 43),
  [COLOR_GREEN]           = RGB565(0, 187, 0),
  [COLOR_YELLOW]          = RGB565(255, 199, 6),
  [COLOR_BLUE]            = RGB565(0, 111, 184),
  [COLOR_MAGENTA]         = RGB565(118, 38, 113),
  [COLOR_CYAN]            = RGB565(44, 181, 233),
  [COLOR_WHITE]           = RGB565(187, 187, 187),
  [COLOR_GRAY]            = RGB565(85, 85, 85),
  [COLOR_BRIGHT_RED]      = RGB565(255, 0, 0),
  [COLOR_BRIGHT_GREEN]    = RGB565(85, 255, 85),
  [COLOR_BRIGHT_YELLOW]   = RGB565(255, 255, 85),
  [COLOR_BRIGHT_BLUE]     = RGB565(0, 0, 255),
  [COLOR_BRIGHT_MAGENTA]  = RGB565(255, 0, 255),
  [COLOR_BRIGHT_CYAN]     = RGB565(0, 255, 255),
  [COLOR_BRIGHT_WHITE]    = RGB565(255, 255, 255),
};

// Naive code to draw a character on the screen pixel-by-pixel. A more
// efficient solution would use boolean operations and a "mask lookup table"
// instead.
// TODO: implement this solution for better performance!
static void
framebuffer_draw_char(struct FrameBuffer *fb,
                      unsigned pos,
                      char c,
                      uint16_t fg,
                      uint16_t bg)
{
  static uint8_t mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };

  uint8_t *glyph;
  uint16_t x0, y0, x, y;

  if (c == '\0')
    c = ' ';
  glyph = &fb->font.bitmap[c * fb->font.glyph_height];

  x0 = (pos % fb->screen->cols) * fb->font.glyph_width;
  y0 = (pos / fb->screen->cols) * fb->font.glyph_height;

  for (x = 0; x < fb->font.glyph_width; x++)
    for (y = 0; y < fb->font.glyph_height; y++)
      fb->base[fb->width * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? colors[fg]
        : colors[bg];
}
