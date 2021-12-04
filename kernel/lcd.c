// See PrimeCell Color LCD Controller (PL111) Technical Reference Manual.

#include <stdint.h>
#include <string.h>

#include "lcd.h"
#include "memlayout.h"
#include "page.h"
#include "vm.h"

// LCD base memory address
#define LCD_BASE          0x10020000      

// LCD registers, shifted right by 2 bits for use as uint32_t[] 
#define LCD_TIMING0       (0x000 / 4)     // Horizontal Axis Panel Control
#define LCD_TIMING1       (0x004 / 4)     // Vertical Axis Panel Control
#define LCD_TIMING2       (0x008 / 4)     // Clock and Signal Polarity Control
#define LCD_UPBASE        (0x010 / 4)     // Upper Panel Frame Base Address
#define LCD_CONTROL       (0x018 / 4)     // LCD Control
#define   LCD_EN            (1U << 0)     //   CLCDC Enable
#define   LCD_BPP16         (4U << 1)     //   16 bits per pixel
#define   LCD_PWR           (1U << 11)    //   LCD Power Enable

// Display dimensions, in pixels
#define DISPLAY_WIDTH     640
#define DISPLAY_HEIGHT    480

// Single font character dimensions, in pixels
#define GLYPH_WIDTH       8
#define GLYPH_HEIGHT      16

// Text buffer dimensions, in characters
#define BUF_WIDTH         (DISPLAY_WIDTH / GLYPH_WIDTH)
#define BUF_HEIGHT        (DISPLAY_HEIGHT / GLYPH_HEIGHT)
#define BUF_SIZE          (BUF_WIDTH * BUF_HEIGHT)            

static void lcd_draw_char(char, int, uint16_t, uint16_t);
static void lcd_flush(void);
static void lcd_update_cursor(void);
static void lcd_scroll_screen(void);
static void lcd_esc_handle(char);
static void lcd_esc_parse(char);

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be equal to PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

// Address of the embedded PSF font file
extern uint8_t _binary_kernel_vga_font_psf_start[];
// Address of the bitmap to draw characters on the screen
static uint8_t *font;

// Text-mode buffer
static struct {
  char     ch;
  uint16_t fg;
  uint16_t bg;
} buf[BUF_SIZE];

static int buf_pos;               // Current position in the text buffer
static int cur_pos;               // Text-mode cursor position
static int cur_fg;                // Current foreground color
static int cur_bg;                // Current background color

#define ESC_MAX_PARAM   16

static int esc_state = 0;
static int esc_params[ESC_MAX_PARAM];
static int esc_cur_param;

static uint16_t *fb;              // Framebuffer base address
static volatile uint32_t *lcd;    // LCD registers base address

// 15-bit RGB colors
static uint16_t ansi_colors[] = {
  0x0000,       // Black
  0x0019,       // Red
  0x0320,       // Green
  0x0339,       // Yellow
  0x7400,       // Blue
  0x6419,       // Magenta
  0x6720,       // Cyan
  0x739c,       // White
  0x3def,       // Bright Black (Gray)
  0x001f,       // Bright Red
  0x1386,       // Bright Green
  0x13bd,       // Bright Yellow
  0x7d6b,       // Bright Blue
  0x7c1f,       // Bright Magenta
  0x7bc2,       // Bright Cyan
  0x7fff,       // Bright White
};

/**
 * Initialize the LCD driver.
 */
void
lcd_init(void)
{
  struct PageInfo *p;
  struct PsfHeader *psf_header;
  int i;
  
  psf_header = (struct PsfHeader *) _binary_kernel_vga_font_psf_start;

  // Make sure the font file is valid.
  if (psf_header->magic != PSF_MAGIC || psf_header->charsize != GLYPH_HEIGHT)
    return;

  font = (uint8_t *) (psf_header + 1);

  // Allocate the frame buffer.
  if ((p = page_alloc_block(8, PAGE_ALLOC_ZERO)) == NULL)
    return;

  fb = (uint16_t *) page2kva(p);
  p->ref_count++;

  lcd = (volatile uint32_t *) vm_map_mmio(LCD_BASE, 4096);

  // Set display resolution: VGA (640x480) on VGA.
  lcd[LCD_TIMING0] = 0x3F1F3F9C;
  lcd[LCD_TIMING1] = 0x090B61DF;
  lcd[LCD_TIMING2] = 0x067F1800;

  // Set the frame buffer physical base address.
  lcd[LCD_UPBASE] = PADDR(fb);

  // Enable LCD, 16 bpp.
  lcd[LCD_CONTROL] = LCD_EN | LCD_BPP16 | LCD_PWR;

  // White on black
  cur_fg = 7;
  cur_bg = 0;

  buf_pos = 0;
  cur_pos = 0;

  // Clear the screen
  for (i = 0; i < BUF_SIZE; i++) {
    buf[i].ch = ' ';
    buf[i].fg = ansi_colors[cur_fg];
    buf[i].bg = ansi_colors[cur_bg];
  }
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
lcd_putc(char c)
{
  if (!lcd)
    return;

  if (esc_state) {
    lcd_esc_parse(c);
    return;
  }

  if (c == '\x1b') {
    esc_state = c;
    return;
  }

  switch (c) {
  case '\n':
    buf_pos += BUF_WIDTH;
    // fall through
  case '\r':
    buf_pos -= buf_pos % BUF_WIDTH;
    break;

  case '\b':
    if (buf_pos > 0) {
      buf_pos--;
      buf[buf_pos].ch = ' ';
      buf[buf_pos].fg = ansi_colors[cur_fg];
      buf[buf_pos].bg = ansi_colors[cur_bg];
      lcd_flush();
    }
    break;

  case '\t':
    do {
      buf[buf_pos].ch = ' ';
      buf[buf_pos].fg = ansi_colors[cur_fg];
      buf[buf_pos].bg = ansi_colors[cur_bg];
      buf_pos++;
    } while ((buf_pos % 4) != 0);
    lcd_flush();
    break;

  default:
    buf[buf_pos].ch = c;
    buf[buf_pos].fg = ansi_colors[cur_fg];
    buf[buf_pos].bg = ansi_colors[cur_bg];
    buf_pos++;
    lcd_flush();
    break;
  }

  if (buf_pos >= BUF_SIZE)
    lcd_scroll_screen();

  lcd_update_cursor();
}

static void
lcd_flush(void)
{
  uint16_t i;

  i = cur_pos;

  while (i > buf_pos) {
    i--;
    lcd_draw_char(buf[i].ch, i, buf[i].fg, buf[i].bg);
  }

  while (i < buf_pos) {
    lcd_draw_char(buf[i].ch, i, buf[i].fg, buf[i].bg);
    i++;
  }
}

// Erase the old text-mode cursor and draw at the new position.
// The current cursor position is indicated by inverting the foreground and
// the background colors of the character in the corresponding position.
static void
lcd_update_cursor(void)
{
  lcd_draw_char(buf[cur_pos].ch, cur_pos, buf[cur_pos].fg, buf[cur_pos].bg);
  lcd_draw_char(buf[buf_pos].ch, buf_pos, buf[buf_pos].bg, buf[buf_pos].fg);

  cur_pos = buf_pos;
}

// Naive code to draw a character on the screen pixel-by-pixel. A more
// efficient solution would use boolean operations and a "mask lookup table"
// instead.
// TODO: implement this solution for better performance!
static void
lcd_draw_char(char c, int pos, uint16_t fg, uint16_t bg)
{
  static uint8_t mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };

  uint8_t *glyph;
  uint16_t x0, y0, x, y;

  if (c == '\0')
    c = ' ';
  glyph = &font[c * GLYPH_HEIGHT];

  x0 = (pos % BUF_WIDTH) * GLYPH_WIDTH;
  y0 = (pos / BUF_WIDTH) * GLYPH_HEIGHT;

  for (x = 0; x < GLYPH_WIDTH; x++)
    for (y = 0; y < GLYPH_HEIGHT; y++)
      fb[DISPLAY_WIDTH * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x]) ? fg : bg;
}

// Move the framebuffer contents one text row up and fill the bottom row with
// the specified background color.
static void
lcd_fb_scroll(uint16_t bg)
{
  int i;

  for (i = 0; i < DISPLAY_WIDTH * (DISPLAY_HEIGHT - GLYPH_HEIGHT); i++)
    fb[i] = fb[i + DISPLAY_WIDTH * GLYPH_HEIGHT];

  for ( ; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
    fb[i] = bg;
}

static void
lcd_scroll_screen(void)
{
  int i;

  for (i = 0; i < BUF_SIZE - BUF_WIDTH; i++)
    buf[i] = buf[i + BUF_WIDTH];

  for ( ; i < BUF_SIZE; i++) {
    buf[i].ch = ' ';
    buf[i].fg = ansi_colors[cur_fg];
    buf[i].bg = ansi_colors[cur_bg];
  }

  lcd_fb_scroll(cur_bg);

  buf_pos -= BUF_WIDTH;
  cur_pos -= BUF_WIDTH;
}

static void
lcd_esc_handle(char c)
{
  int i;

  switch (c) {
  case 'm':
    // Set graphics mode
    for (i = 0; (i <= esc_cur_param) && (i < ESC_MAX_PARAM); i++) {
      switch (esc_params[i]) {
      case 0:
        // All attributes off
        cur_bg = 0;
        cur_fg = 7;
        break;
      case 1:
        // Bold on
        cur_fg |= 0x8;
        break;
      default:
        if ((esc_params[i] >= 30) && (esc_params[i] <= 37)) {
          // Set foreground color
          cur_fg = (cur_fg & ~0x7) | (esc_params[i] - 30);
        } else if ((esc_params[i] >= 40) && (esc_params[i] <= 47)) {
          // Set background color
          cur_bg = (cur_bg & ~0x7) | (esc_params[i] - 40);
        }
      }
    }
    break;

  // TODO

  default:
    break;
  }

  esc_state = 0;
}

static void
lcd_esc_parse(char c)
{
  int i;

  switch (esc_state) {
	case '\x1b':
		if (c == '[') {
			esc_state = c;
      esc_cur_param = 0;
      for (i = 0; i < ESC_MAX_PARAM; i++)
        esc_params[i] = 0;
		} else {
			esc_state = 0;
		}
		break;
	case '[':
		if (c >= '0' && c <= '9') {
			// Parse the current parameter
			if (esc_cur_param < ESC_MAX_PARAM)
        esc_params[esc_cur_param] = esc_params[esc_cur_param] * 10 + (c - '0');
		} else if (c == ';') {
			// Next parameter
			if (esc_cur_param < ESC_MAX_PARAM)
				esc_cur_param++;
		} else {
			lcd_esc_handle(c);
		}
		break;
	default:
		esc_state = 0;
	}
}
