#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <drivers/console.h>
#include <mm/memlayout.h>
#include <mm/page.h>
#include <mm/vm.h>
#include <types.h>

#include "display.h"
#include "pl111.h"

static struct Pl111 lcd;

// ANSI color codes
#define COLOR_MASK            7
#define COLOR_BLACK           0
#define COLOR_RED             1
#define COLOR_GREEN           2
#define COLOR_YELLOW          3
#define COLOR_BLUE            4
#define COLOR_MAGENTA         5
#define COLOR_CYAN            6
#define COLOR_WHITE           7
#define COLOR_BRIGHT          (COLOR_MASK + 1)
#define COLOR_GRAY            (COLOR_BRIGHT + COLOR_BLACK)
#define COLOR_BRIGHT_RED      (COLOR_BRIGHT + COLOR_RED)
#define COLOR_BRIGHT_GREEN    (COLOR_BRIGHT + COLOR_GREEN)
#define COLOR_BRIGHT_YELLOW   (COLOR_BRIGHT + COLOR_YELLOW)
#define COLOR_BRIGHT_BLUE     (COLOR_BRIGHT + COLOR_BLUE)
#define COLOR_BRIGHT_MAGENTA  (COLOR_BRIGHT + COLOR_MAGENTA)
#define COLOR_BRIGHT_CYAN     (COLOR_BRIGHT + COLOR_CYAN)
#define COLOR_BRIGHT_WHITE    (COLOR_BRIGHT + COLOR_WHITE)

static void display_buf_putc(unsigned, char);

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be equal to PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

static unsigned cursor_pos;              // Current cursor position
static int cursor_visible;
static int fg_color = COLOR_WHITE;      // Current foreground color
static int bg_color = COLOR_BLACK;      // Current background color

#define ESC_MAX_PARAM   16            // The maximum number of esc parameters

enum ParserState {
  PARSER_NORMAL,
  PARSER_ESC,
  PARSER_CSI,
};

// State of the esc sequence parser
static enum ParserState parser_state = PARSER_NORMAL;          

static int esc_params[ESC_MAX_PARAM]; // The esc sequence parameters
static int esc_cur_param;             // Index of the current esc parameter

#define DEFAULT_BUF_WIDTH   80
#define DEFAULT_BUF_HEIGHT  30

#define DEFAULT_FB_WIDTH    640
#define DEFAULT_FB_HEIGHT   480

static struct {
  struct {
    unsigned ch : 8;
    unsigned fg : 4;
    unsigned bg : 4;
  } data[DEFAULT_BUF_WIDTH * DEFAULT_BUF_HEIGHT];
  unsigned width;
  unsigned height;
  unsigned pos;
} buf;

static struct {
  uint8_t *bitmap;
  uint8_t  glyph_width;
  uint8_t  glyph_height;
} font;

static uint16_t *fb_base;
static unsigned fb_width;
static unsigned fb_height;

static void display_handle_esc(char);
static void display_erase_cursor(void);
static void display_draw_cursor(void);
static void display_draw_char(unsigned , char, uint16_t, uint16_t);
static int  display_load_font(const void *);
static void display_scroll_down(unsigned);
static void display_buf_flush(void);
static void display_echo(char);

/**
 * Initialize the display driver.
 */
int
display_init(void)
{
  extern uint8_t _binary_kernel_drivers_vga_font_psf_start[];

  struct Page *page;
  unsigned i;

  buf.width  = DEFAULT_BUF_WIDTH;
  buf.height = DEFAULT_BUF_HEIGHT;
  for (i = 0; i < buf.width * buf.height; i++) {
    buf.data[i].ch = ' ';
    buf.data[i].fg = COLOR_WHITE;
    buf.data[i].bg = COLOR_BLACK;
  }

  // Allocate the frame buffer.
  if ((page = page_alloc_block(8, PAGE_ALLOC_ZERO)) == NULL)
    return -ENOMEM;

  fb_width  = DEFAULT_FB_WIDTH;
  fb_height = DEFAULT_FB_HEIGHT;
  fb_base   = (uint16_t *) page2kva(page);
  page->ref_count++;

  display_load_font(_binary_kernel_drivers_vga_font_psf_start);

  pl111_init(&lcd, PA2KVA(PHYS_LCD), page2pa(page), PL111_RES_VGA);

  return 0;
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
display_putc(char c)
{ 
  int i;

  switch (parser_state) {
  case PARSER_NORMAL:
    if (c == '\x1b')
      parser_state = PARSER_ESC;
    else
      display_echo(c);
    break;

	case PARSER_ESC:
		if (c == '[') {
			parser_state = PARSER_CSI;
      esc_cur_param = 0;
      for (i = 0; i < ESC_MAX_PARAM; i++)
        esc_params[i] = 0;
		} else {
			parser_state = PARSER_NORMAL;
		}
		break;

	case PARSER_CSI:
		if (c >= '0' && c <= '9') {
			// Parse the current parameter
			if (esc_cur_param < ESC_MAX_PARAM)
        esc_params[esc_cur_param] = esc_params[esc_cur_param] * 10 + (c - '0');
		} else if (c == ';') {
			// Next parameter
			if (esc_cur_param < ESC_MAX_PARAM)
				esc_cur_param++;
		} else {
			display_handle_esc(c);
      parser_state = PARSER_NORMAL;
		}
		break;

	default:
		parser_state = PARSER_NORMAL;
    break;
	}
}

/**
 * Send any buffered characters to the video memory and draw the cursor.
 */
void
display_flush(void)
{
  display_buf_flush();

  if (!cursor_visible) {
    cursor_visible = 1;
    display_draw_cursor();
  }
}

/*
 * ----------------------------------------------------------------------------
 * Simple parser for ANSI escape sequences
 * ----------------------------------------------------------------------------
 * 
 * The following escape sequences are currently supported:
 *   CSI n m - Select Graphic Rendition
 *
 */

// Handle the escape sequence terminated by the final character c.
static void
display_handle_esc(char c)
{
  int i, tmp;

  switch (c) {
  // Set Graphic Rendition
  case 'm':       
    for (i = 0; (i <= esc_cur_param) && (i < ESC_MAX_PARAM); i++) {
      switch (esc_params[i]) {
      case 0:
        // All attributes off
        bg_color = COLOR_BLACK;
        fg_color = COLOR_WHITE;
        break;
      case 1:
        // Bold on
        fg_color |= COLOR_BRIGHT;
        break;
      case 7:
        // Reverse video
        tmp = bg_color;
        bg_color = fg_color;
        fg_color = tmp;
        break;
      case 39:
        // Default foreground color (white)
        fg_color = (fg_color & ~COLOR_MASK) | COLOR_WHITE;
        break;
      case 49:
        // Default background color (black)
        bg_color = (bg_color & ~COLOR_MASK) | COLOR_BLACK;
        break;
      default:
        if ((esc_params[i] >= 30) && (esc_params[i] <= 37)) {
          // Set foreground color
          fg_color = (fg_color & ~COLOR_MASK) | (esc_params[i] - 30);
        } else if ((esc_params[i] >= 40) && (esc_params[i] <= 47)) {
          // Set background color
          bg_color = (bg_color & ~COLOR_MASK) | (esc_params[i] - 40);
        }
      }
    }
    break;

  // TODO: handle other control sequences here

  default:
    break;
  }
}

static int
display_load_font(const void *addr)
{
  struct PsfHeader *psf;

  psf = (struct PsfHeader *) addr;
  if ((psf->magic != PSF_MAGIC) || (psf->charsize != 16))
    return -EINVAL;

  font.bitmap       = (uint8_t *) (psf + 1);
  font.glyph_width  = 8;
  font.glyph_height = psf->charsize;

  for (unsigned i = 0; i < buf.width * buf.height; i++)
    display_draw_char(i, buf.data[i].ch, buf.data[i].fg, buf.data[i].bg);
  
  if (cursor_visible)
    display_draw_cursor();

  return 0;
}

static void
display_buf_flush(void)
{
  if (cursor_pos < buf.pos) {
    cursor_visible = 0;

    for ( ; cursor_pos < buf.pos; cursor_pos++)
      display_draw_char(cursor_pos,
                        buf.data[cursor_pos].ch,
                        buf.data[cursor_pos].fg,
                        buf.data[cursor_pos].bg);
  } else if (cursor_pos > buf.pos) {
    if (cursor_visible) {
      display_erase_cursor();
      cursor_visible = 0;
    }

    for ( ; cursor_pos > buf.pos; cursor_pos--)
      display_draw_char(cursor_pos,
                        buf.data[cursor_pos].ch,
                        buf.data[cursor_pos].fg,
                        buf.data[cursor_pos].bg);
  }
}

#define DISPLAY_TAB_WIDTH  4

static void
display_echo(char c)
{ 
  switch (c) {
  case '\n':
    display_buf_flush();

    if (cursor_visible) {
      cursor_visible = 0;
      display_erase_cursor();
    }

    buf.pos += buf.width;
    buf.pos -= buf.pos % buf.width;
    cursor_pos = buf.pos;
    break;

  case '\r':
    display_buf_flush();

    if (cursor_visible) {
      cursor_visible = 0;
      display_erase_cursor();
    }

    buf.pos -= buf.pos % buf.width;
    cursor_pos = buf.pos;
    break;

  case '\b':
    if (buf.pos > 0)
      display_buf_putc(--buf.pos, ' ');
    break;

  case '\t':
    do {
      display_buf_putc(buf.pos++, ' ');
    } while ((buf.pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  default:
    display_buf_putc(buf.pos++, c);
    break;
  }

  if (buf.pos >= buf.width * buf.height)
    display_scroll_down(1);
}

static void
display_scroll_down(unsigned n)
{
  unsigned i;

  display_buf_flush();

  memmove(&buf.data[0], &buf.data[buf.width * n],
          sizeof(buf.data[0]) * (buf.width * (buf.height - n)));

  for (i = buf.width * (buf.height - n); i < buf.width * buf.height; i++) {
    buf.data[i].ch = ' ';
    buf.data[i].fg = COLOR_WHITE;
    buf.data[i].bg = COLOR_BLACK;
  }

  buf.pos -= n * buf.width;

  if (cursor_pos < buf.width * n) {
    cursor_visible = 0;
    cursor_pos = 0;
  } else {
    cursor_pos -= buf.width * n;
  }

  memmove(&fb_base[0], &fb_base[fb_width * font.glyph_height * n],
          sizeof(fb_base[0]) * fb_width * (fb_height - font.glyph_height * n));
  memset(&fb_base[fb_width * (fb_height - font.glyph_height * n)], 0,
         sizeof(fb_base[0]) * fb_width * font.glyph_height * n);
}

static void
display_buf_putc(unsigned i, char c)
{
  buf.data[i].ch = c;
  buf.data[i].fg = fg_color & 0xF;
  buf.data[i].bg = bg_color & 0xF;
}

static void
display_erase_cursor(void)
{
  display_draw_char(cursor_pos, buf.data[cursor_pos].ch,
                    buf.data[cursor_pos].fg, buf.data[cursor_pos].bg);
}

static void
display_draw_cursor(void)
{
  display_draw_char(cursor_pos, buf.data[cursor_pos].ch,
                    buf.data[cursor_pos].bg, buf.data[cursor_pos].fg);
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

  x0 = (pos % buf.width) * font.glyph_width;
  y0 = (pos / buf.width) * font.glyph_height;

  for (x = 0; x < font.glyph_width; x++)
    for (y = 0; y < font.glyph_height; y++)
      fb_base[fb_width * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? colors[fg]
        : colors[bg];
}
