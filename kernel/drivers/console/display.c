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
static void display_erase(unsigned, unsigned);

/**
 * Initialize the display driver.
 */
int
display_init(void)
{
  extern uint8_t _binary_kernel_drivers_console_vga_font_psf_start[];

  struct Page *page;
  unsigned i;

  console_current.out.fg_color      = COLOR_WHITE;
  console_current.out.bg_color      = COLOR_BLACK;
  console_current.out.state         = PARSER_NORMAL;
  console_current.out.esc_cur_param = -1;

  console_current.out.cols  = CONSOLE_COLS;
  console_current.out.rows = CONSOLE_ROWS;
  for (i = 0; i < console_current.out.cols * console_current.out.rows; i++) {
    console_current.out.buf[i].ch = ' ';
    console_current.out.buf[i].fg = COLOR_WHITE;
    console_current.out.buf[i].bg = COLOR_BLACK;
  }

  // Allocate the frame buffer.
  if ((page = page_alloc_block(8, PAGE_ALLOC_ZERO)) == NULL)
    return -ENOMEM;

  fb_width  = DEFAULT_FB_WIDTH;
  fb_height = DEFAULT_FB_HEIGHT;
  fb_base   = (uint16_t *) page2kva(page);
  page->ref_count++;

  display_load_font(_binary_kernel_drivers_console_vga_font_psf_start);

  pl111_init(&lcd, PA2KVA(PHYS_LCD), page2pa(page), PL111_RES_VGA);

  return 0;
}

void
dump(unsigned c)
{
  const char sym[] = "0123456789ABCDEF";

  display_echo('~');
  display_echo('~');
  display_echo('~');

  display_echo(sym[(c >> 28) & 0xF]);
  display_echo(sym[(c >> 24) & 0xF]);
  display_echo(sym[(c >> 20) & 0xF]);
  display_echo(sym[(c >> 16) & 0xF]);
  display_echo(sym[(c >> 12) & 0xF]);
  display_echo(sym[(c >> 8) & 0xF]);
  display_echo(sym[(c >> 4) & 0xF]);
  display_echo(sym[(c >> 0) & 0xF]);

  display_echo('\n');
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

  switch (console_current.out.state) {
  case PARSER_NORMAL:
    if (c == '\x1b')
      console_current.out.state = PARSER_ESC;
    else
      display_echo(c);
    break;

	case PARSER_ESC:
		if (c == '[') {
			console_current.out.state = PARSER_CSI;
      console_current.out.esc_cur_param = -1;
      console_current.out.esc_question = 0;
      for (i = 0; i < CONSOLE_ESC_MAX; i++)
        console_current.out.esc_params[i] = 0;
		} else {
			console_current.out.state = PARSER_NORMAL;
		}
		break;

	case PARSER_CSI:
    if (c == '?') {
      if (console_current.out.esc_cur_param == -1) {
        console_current.out.esc_question = 1;
      } else {
        console_current.out.state = PARSER_NORMAL;
      }
    } else {
      if (c >= '0' && c <= '9') {
        if (console_current.out.esc_cur_param == -1)
          console_current.out.esc_cur_param = 0;

        // Parse the current parameter
        if (console_current.out.esc_cur_param < CONSOLE_ESC_MAX)
          console_current.out.esc_params[console_current.out.esc_cur_param] = console_current.out.esc_params[console_current.out.esc_cur_param] * 10 + (c - '0');
      } else if (c == ';') {
        // Next parameter
        if (console_current.out.esc_cur_param < CONSOLE_ESC_MAX)
          console_current.out.esc_cur_param++;
      } else {
        if (console_current.out.esc_cur_param < CONSOLE_ESC_MAX)
          console_current.out.esc_cur_param++;

        display_handle_esc(c);
        console_current.out.state = PARSER_NORMAL;
      }
    }
		break;

	default:
		console_current.out.state = PARSER_NORMAL;
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

  if (!console_current.out.cursor_visible) {
    console_current.out.cursor_visible = 1;
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
  unsigned n, m;

  display_flush();

  switch (c) {

  // Cursor Up
  case 'A':
    n = (console_current.out.esc_cur_param == 0) ? 1 : console_current.out.esc_params[0];
    n = MIN(n, console_current.out.pos / console_current.out.cols);
    console_current.out.pos -= n * console_current.out.cols;
    display_flush();
    break;
  
  // Cursor Down
  case 'B':
    n = (console_current.out.esc_cur_param == 0) ? 1 : console_current.out.esc_params[0];
    n = MIN(n, console_current.out.rows - console_current.out.pos / console_current.out.cols - 1);
    console_current.out.pos += n * console_current.out.cols;
    display_flush();
    break;

  // Cursor Forward
  case 'C':
    for (;;);
    n = (console_current.out.esc_cur_param == 0) ? 1 : console_current.out.esc_params[0];
    n = MIN(n, console_current.out.cols - console_current.out.pos % console_current.out.cols - 1);
    console_current.out.pos += n;
    display_flush();
    break;

  // Cursor Back
  case 'D':
    n = (console_current.out.esc_cur_param == 0) ? 1 : console_current.out.esc_params[0];
    n = MIN(n, console_current.out.pos % console_current.out.cols);
    console_current.out.pos -= n;
    display_flush();
    break;

  // Cursor Horizontal Absolute
  case 'G':
    n = (console_current.out.esc_cur_param < 1) ? 0 : console_current.out.esc_params[0];
    n = MIN(n, console_current.out.cols - 1);
    console_current.out.pos -= console_current.out.pos % console_current.out.cols;
    console_current.out.pos += n;
    display_flush();
    break;

  // Cursor Position
  case 'H':
    n = (console_current.out.esc_cur_param < 1) ? 0 : console_current.out.esc_params[0];
    m = (console_current.out.esc_cur_param < 2) ? 0 : console_current.out.esc_params[1];
    n = MIN(n, console_current.out.rows - 1);
    m = MIN(m, console_current.out.cols - 1);
    console_current.out.pos = n * console_current.out.cols + m;
    display_flush();
    break;

  // Erase in Display
  case 'J':
    n = (console_current.out.esc_cur_param < 1) ? 0 : console_current.out.esc_params[0];
    if (n == 0) {
      display_erase(console_current.out.pos, CONSOLE_COLS * CONSOLE_ROWS - 1);
    } else if (n == 1) {
      display_erase(0, console_current.out.pos);
    } else if (n == 2) {
      display_erase(0, CONSOLE_COLS * CONSOLE_ROWS - 1);
    }
    display_flush();
    break;

  // Erase in Line
  case 'K':
    n = (console_current.out.esc_cur_param < 1) ? 0 : console_current.out.esc_params[0];
    if (n == 0) {
      display_erase(console_current.out.pos, console_current.out.pos - console_current.out.pos % CONSOLE_COLS + CONSOLE_COLS - 1);
    } else if (n == 1) {
      display_erase(console_current.out.pos - console_current.out.pos % CONSOLE_COLS, console_current.out.pos);
    } else if (n == 2) {
      display_erase(console_current.out.pos - console_current.out.pos % CONSOLE_COLS, console_current.out.pos - console_current.out.pos % CONSOLE_COLS + CONSOLE_COLS - 1);
    }
    display_flush();
    break;

  // Cursor Vertical Position
  case 'd':
    n = (console_current.out.esc_cur_param < 1) ? 0 : console_current.out.esc_params[0];
    n = MIN(n, console_current.out.rows - 1);
    console_current.out.pos = n * console_current.out.cols + m;
    display_flush();
    break;

  // case 't':
  //   break;

  // case 'h':
  //   if (console_current.out.esc_params[0] != 1049)
  //     dump('a');
  //   break;

  // case 'l':
  //   if (console_current.out.esc_params[0] != 1049)
  //     dump('a');
  //   break;
    

  // Set Graphic Rendition
  case 'm':
    if (console_current.out.esc_cur_param == 0) {
      // All attributes off
      console_current.out.bg_color = COLOR_BLACK;
      console_current.out.fg_color = COLOR_WHITE;
      break;
    }

    for (i = 0; (i < console_current.out.esc_cur_param) && (i < CONSOLE_ESC_MAX); i++) {
      switch (console_current.out.esc_params[i]) {
      case 0:
        // All attributes off
        console_current.out.bg_color = COLOR_BLACK;
        console_current.out.fg_color = COLOR_WHITE;
        break;
      case 1:
        // Bold on
        console_current.out.fg_color |= COLOR_BRIGHT;
        break;
      case 7:
        // Reverse video
        tmp = console_current.out.bg_color;
        console_current.out.bg_color = console_current.out.fg_color;
        console_current.out.fg_color = tmp;
        break;
      case 39:
        // Default foreground color (white)
        console_current.out.fg_color = (console_current.out.fg_color & ~COLOR_MASK) | COLOR_WHITE;
        break;
      case 49:
        // Default background color (black)
        console_current.out.bg_color = (console_current.out.bg_color & ~COLOR_MASK) | COLOR_BLACK;
        break;
      default:
        if ((console_current.out.esc_params[i] >= 30) && (console_current.out.esc_params[i] <= 37)) {
          // Set foreground color
          console_current.out.fg_color = (console_current.out.fg_color & ~COLOR_MASK) | (console_current.out.esc_params[i] - 30);
        } else if ((console_current.out.esc_params[i] >= 40) && (console_current.out.esc_params[i] <= 47)) {
          // Set background color
          console_current.out.bg_color = (console_current.out.bg_color & ~COLOR_MASK) | (console_current.out.esc_params[i] - 40);
        }
      }
    }
    display_flush();
    break;

  case 'b':
    for (n = 0; n < console_current.out.esc_params[0]; n++)
      display_echo(console_current.out.buf[console_current.out.pos - 1].ch);
    display_flush();
    break;

  // TODO: handle other control sequences here

  default:
    dump(c);
    dump(console_current.out.esc_params[0]);
    for (;;);

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

  for (unsigned i = 0; i < console_current.out.cols * console_current.out.rows; i++)
    display_draw_char(i, console_current.out.buf[i].ch, console_current.out.buf[i].fg, console_current.out.buf[i].bg);
  
  if (console_current.out.cursor_visible)
    display_draw_cursor();

  return 0;
}

static void
display_erase(unsigned from, unsigned to)
{
  while (from <= to) {
    display_buf_putc(from, ' ');
    display_draw_char(from,
                      console_current.out.buf[from].ch,
                      console_current.out.buf[from].fg,
                      console_current.out.buf[from].bg);
    from++;
  }

  if (console_current.out.cursor_visible)
    display_draw_cursor();
}

static void
display_buf_flush(void)
{
  if (console_current.out.cursor_pos < console_current.out.pos) {
    console_current.out.cursor_visible = 0;

    for ( ; console_current.out.cursor_pos < console_current.out.pos; console_current.out.cursor_pos++)
      display_draw_char(console_current.out.cursor_pos,
                        console_current.out.buf[console_current.out.cursor_pos].ch,
                        console_current.out.buf[console_current.out.cursor_pos].fg,
                        console_current.out.buf[console_current.out.cursor_pos].bg);
  } else if (console_current.out.cursor_pos > console_current.out.pos) {
    if (console_current.out.cursor_visible) {
      display_erase_cursor();
      console_current.out.cursor_visible = 0;
    }

    for ( ; console_current.out.cursor_pos > console_current.out.pos; console_current.out.cursor_pos--)
      display_draw_char(console_current.out.cursor_pos,
                        console_current.out.buf[console_current.out.cursor_pos].ch,
                        console_current.out.buf[console_current.out.cursor_pos].fg,
                        console_current.out.buf[console_current.out.cursor_pos].bg);
  }
}

#define DISPLAY_TAB_WIDTH  4

static void
display_echo(char c)
{ 
  switch (c) {
  case '\n':
    display_buf_flush();

    if (console_current.out.cursor_visible) {
      console_current.out.cursor_visible = 0;
      display_erase_cursor();
    }

    console_current.out.pos += console_current.out.cols;
    console_current.out.pos -= console_current.out.pos % console_current.out.cols;
    console_current.out.cursor_pos = console_current.out.pos;
    break;

  case '\r':
    display_buf_flush();

    if (console_current.out.cursor_visible) {
      console_current.out.cursor_visible = 0;
      display_erase_cursor();
    }

    console_current.out.pos -= console_current.out.pos % console_current.out.cols;
    console_current.out.cursor_pos = console_current.out.pos;
    break;

  case '\b':
    if (console_current.out.pos > 0)
      display_buf_putc(--console_current.out.pos, ' ');
    break;

  case '\t':
    do {
      display_buf_putc(console_current.out.pos++, ' ');
    } while ((console_current.out.pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  default:
    display_buf_putc(console_current.out.pos++, c);
    break;
  }

  if (console_current.out.pos >= console_current.out.cols * console_current.out.rows)
    display_scroll_down(1);
}

static void
display_scroll_down(unsigned n)
{
  unsigned i;

  display_buf_flush();

  memmove(&console_current.out.buf[0], &console_current.out.buf[console_current.out.cols * n],
          sizeof(console_current.out.buf[0]) * (console_current.out.cols * (console_current.out.rows - n)));

  for (i = console_current.out.cols * (console_current.out.rows - n); i < console_current.out.cols * console_current.out.rows; i++) {
    console_current.out.buf[i].ch = ' ';
    console_current.out.buf[i].fg = COLOR_WHITE;
    console_current.out.buf[i].bg = COLOR_BLACK;
  }

  console_current.out.pos -= n * console_current.out.cols;

  if (console_current.out.cursor_pos < console_current.out.cols * n) {
    console_current.out.cursor_visible = 0;
    console_current.out.cursor_pos = 0;
  } else {
    console_current.out.cursor_pos -= console_current.out.cols * n;
  }

  memmove(&fb_base[0], &fb_base[fb_width * font.glyph_height * n],
          sizeof(fb_base[0]) * fb_width * (fb_height - font.glyph_height * n));
  memset(&fb_base[fb_width * (fb_height - font.glyph_height * n)], 0,
         sizeof(fb_base[0]) * fb_width * font.glyph_height * n);
}

static void
display_buf_putc(unsigned i, char c)
{
  console_current.out.buf[i].ch = c;
  console_current.out.buf[i].fg = console_current.out.fg_color & 0xF;
  console_current.out.buf[i].bg = console_current.out.bg_color & 0xF;
}

static void
display_erase_cursor(void)
{
  display_draw_char(console_current.out.cursor_pos, console_current.out.buf[console_current.out.cursor_pos].ch,
                    console_current.out.buf[console_current.out.cursor_pos].fg, console_current.out.buf[console_current.out.cursor_pos].bg);
}

static void
display_draw_cursor(void)
{
  display_draw_char(console_current.out.cursor_pos, console_current.out.buf[console_current.out.cursor_pos].ch,
                    console_current.out.buf[console_current.out.cursor_pos].bg, console_current.out.buf[console_current.out.cursor_pos].fg);
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

  x0 = (pos % console_current.out.cols) * font.glyph_width;
  y0 = (pos / console_current.out.cols) * font.glyph_height;

  for (x = 0; x < font.glyph_width; x++)
    for (y = 0; y < font.glyph_height; y++)
      fb_base[fb_width * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? colors[fg]
        : colors[bg];
}
