#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <kernel/drivers/console.h>
#include <kernel/mm/memlayout.h>
#include <kernel/page.h>
#include <kernel/vm.h>
#include <kernel/types.h>

#include "display.h"
#include "pl111.h"

static struct Pl111 lcd;

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be equal to PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

static struct {
  uint8_t *bitmap;
  uint8_t  glyph_width;
  uint8_t  glyph_height;
} font;

static uint16_t *fb_base;
static unsigned fb_width;
static unsigned fb_height;

unsigned display_pos;

static unsigned cursor_pos;
static int      cursor_visible;

static void display_draw_cursor(void);
static void display_erase_cursor(void);
static void display_draw_char(unsigned, char, uint16_t, uint16_t);

void
display_draw_char_at(struct Console *console, unsigned i)
{
  display_draw_char(i,
                    console->out.buf[i].ch,
                    console->out.buf[i].fg,
                    console->out.buf[i].bg);

  if (i == cursor_pos)
    cursor_visible = 0;
}

/**
 * Initialize the display driver.
 */
int
display_init(void)
{
  extern uint8_t _binary_kernel_drivers_console_vga_font_psf_start[];

  struct Page *page;
  struct PsfHeader *psf;

  psf = (struct PsfHeader *) _binary_kernel_drivers_console_vga_font_psf_start;
  if ((psf->magic != PSF_MAGIC) || (psf->charsize != 16))
    return -EINVAL;

  font.bitmap       = (uint8_t *) (psf + 1);
  font.glyph_width  = 8;
  font.glyph_height = psf->charsize;

  // Allocate the frame buffer.
  if ((page = page_alloc_block(8, PAGE_ALLOC_ZERO)) == NULL)
    return -ENOMEM;

  fb_width  = DEFAULT_FB_WIDTH;
  fb_height = DEFAULT_FB_HEIGHT;
  fb_base   = (uint16_t *) page2kva(page);
  page->ref_count++;

  pl111_init(&lcd, PA2KVA(PHYS_LCD), page2pa(page), PL111_RES_VGA);

  return 0;
}

void
display_update(struct Console *console)
{
  unsigned i;

  for (i = 0; i < console->out.cols * console->out.rows; i++)
    display_draw_char_at(console, i);

  display_pos = console->out.pos;
  cursor_pos  = display_pos;

  display_draw_cursor();
}

void
display_update_cursor(struct Console *console)
{
  display_pos = console->out.pos;

  if (cursor_pos != display_pos) {
    display_erase_cursor();
    cursor_pos = display_pos;
  }

  display_draw_cursor();
}

void
display_erase(struct Console *console, unsigned from, unsigned to)
{
  unsigned i;

  for (i = from; i <= to; i++) {
    display_draw_char(i, ' ', console->out.buf[i].fg, console->out.buf[i].bg);

    if (i == cursor_pos)
      cursor_visible = 0;
  }
}

void
display_flush(struct Console *console)
{
  if (display_pos < console->out.pos) {
    for ( ; display_pos < console->out.pos; display_pos++)
      display_draw_char_at(console, display_pos);
  } else if (display_pos > console->out.pos) {
    for ( ; display_pos > console->out.pos; display_pos--)
      display_draw_char_at(console, display_pos);
  }
}

void
display_scroll_down(struct Console *console, unsigned n)
{
  display_pos = console->out.pos;

  if (cursor_pos < console->out.cols * n) {
    cursor_pos = 0;
    cursor_visible = 0;
  } else {
    cursor_pos -= console->out.cols * n;
  }

  memmove(&fb_base[0], &fb_base[fb_width * font.glyph_height * n],
          sizeof(fb_base[0]) * fb_width * (fb_height - font.glyph_height * n));
  memset(&fb_base[fb_width * (fb_height - font.glyph_height * n)], 0,
        sizeof(fb_base[0]) * fb_width * font.glyph_height * n);
}

static void
display_erase_cursor(void)
{
  if (cursor_visible) {
    display_draw_char(cursor_pos,
                      console_current->out.buf[cursor_pos].ch,
                      console_current->out.buf[cursor_pos].fg,
                      console_current->out.buf[cursor_pos].bg);
    cursor_visible = 0;
  }
}

static void
display_draw_cursor(void)
{
  if (!cursor_visible) {
    display_draw_char(cursor_pos,
                      console_current->out.buf[cursor_pos].ch,
                      console_current->out.buf[cursor_pos].bg,
                      console_current->out.buf[cursor_pos].fg);
    cursor_visible = 1;
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
display_draw_char(unsigned pos, char c, uint16_t fg, uint16_t bg)
{
  static uint8_t mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };

  uint8_t *glyph;
  uint16_t x0, y0, x, y;

  if (c == '\0')
    c = ' ';
  glyph = &font.bitmap[c * font.glyph_height];

  x0 = (pos % console_current->out.cols) * font.glyph_width;
  y0 = (pos / console_current->out.cols) * font.glyph_height;

  for (x = 0; x < font.glyph_width; x++)
    for (y = 0; y < font.glyph_height; y++)
      fb_base[fb_width * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? colors[fg]
        : colors[bg];
}
