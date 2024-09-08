#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <kernel/drivers/console.h>
#include <kernel/drivers/display.h>
#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/types.h>

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be equal to PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

static void display_draw_cursor(struct Display *);
static void display_erase_cursor(struct Display *);
static void display_draw_char(struct Display *, unsigned, char, uint16_t, uint16_t);

void
display_draw_char_at(struct Display *display, struct Screen *console, unsigned i)
{
  display_draw_char(display, 
                    i,
                    console->buf[i].ch,
                    console->buf[i].fg,
                    console->buf[i].bg);

  if (i == display->cursor_pos)
    display->cursor_visible = 0;
}

/**
 * Initialize the display driver.
 */
int
display_init(struct Display *display, void *base)
{
  extern uint8_t _binary_kernel_drivers_console_vga_font_psf_start[];

  struct PsfHeader *psf;

  psf = (struct PsfHeader *) _binary_kernel_drivers_console_vga_font_psf_start;
  if ((psf->magic != PSF_MAGIC) || (psf->charsize != 16))
    return -EINVAL;

  display->font.bitmap       = (uint8_t *) (psf + 1);
  display->font.glyph_width  = 8;
  display->font.glyph_height = psf->charsize;

  display->pos = 0;
  display->cursor_pos = 0;
  display->cursor_visible = 0;

  display->fb_width  = DEFAULT_FB_WIDTH;
  display->fb_height = DEFAULT_FB_HEIGHT;
  display->fb_base   = (uint16_t *) base;

  return 0;
}

void
display_update(struct Display *display, struct Screen *console)
{
  unsigned i;

  for (i = 0; i < console->cols * console->rows; i++)
    display_draw_char_at(display, console, i);

  display->pos = console->pos;
  display->cursor_pos = display->pos;

  display_draw_cursor(display);
}

void
display_update_cursor(struct Display *display, struct Screen *console)
{
  display->pos = console->pos;

  if (display->cursor_pos != display->pos) {
    display_erase_cursor(display);
    display->cursor_pos = display->pos;
  }

  display_draw_cursor(display);
}

void
display_erase(struct Display *display, struct Screen *console, unsigned from, unsigned to)
{
  unsigned i;
  
  for (i = from; i <= to; i++) {
    display_draw_char(display, i, ' ', console->buf[i].fg, console->buf[i].bg);

    if (i == display->cursor_pos)
      display->cursor_visible = 0;
  }
}

void
display_flush(struct Display *display, struct Screen *console)
{
  if (display->pos < console->pos) {
    for ( ; display->pos < console->pos; display->pos++)
      display_draw_char_at(display, console, display->pos);
  } else if (display->pos > console->pos) {
    for ( ; display->pos > console->pos; display->pos--)
      display_draw_char_at(display, console, display->pos);
  }
}

void
display_scroll_down(struct Display *display, struct Screen *console, unsigned n)
{
  
  display->pos = console->pos;

  if (display->cursor_pos < console->cols * n) {
    display->cursor_pos = 0;
    display->cursor_visible = 0;
  } else {
    display->cursor_pos -= console->cols * n;
  }

  memmove(&display->fb_base[0],
          &display->fb_base[display->fb_width * display->font.glyph_height * n],
          sizeof(display->fb_base[0]) * display->fb_width * (display->fb_height - display->font.glyph_height * n));
  memset(&display->fb_base[display->fb_width * (display->fb_height - display->font.glyph_height * n)],
         0,
         sizeof(display->fb_base[0]) * display->fb_width * display->font.glyph_height * n);
}

static void
display_erase_cursor(struct Display *display)
{
  if (display->cursor_visible) {
    display_draw_char(display,
                      display->cursor_pos,
                      tty_current->out->buf[display->cursor_pos].ch,
                      tty_current->out->buf[display->cursor_pos].fg,
                      tty_current->out->buf[display->cursor_pos].bg);
    display->cursor_visible = 0;
  }
}

static void
display_draw_cursor(struct Display *display)
{
  if (!display->cursor_visible) {
    display_draw_char(display, 
                      display->cursor_pos,
                      tty_current->out->buf[display->cursor_pos].ch,
                      tty_current->out->buf[display->cursor_pos].bg,
                      tty_current->out->buf[display->cursor_pos].fg);
    display->cursor_visible = 1;
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
display_draw_char(struct Display *display, unsigned pos, char c, uint16_t fg, uint16_t bg)
{ 
  static uint8_t mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };

  uint8_t *glyph;
  uint16_t x0, y0, x, y;

  if (c == '\0')
    c = ' ';
  glyph = &display->font.bitmap[c * display->font.glyph_height];

  x0 = (pos % tty_current->out->cols) * display->font.glyph_width;
  y0 = (pos / tty_current->out->cols) * display->font.glyph_height;

  for (x = 0; x < display->font.glyph_width; x++)
    for (y = 0; y < display->font.glyph_height; y++)
      display->fb_base[display->fb_width * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? colors[fg]
        : colors[bg];
}
