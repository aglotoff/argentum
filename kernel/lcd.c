// See PrimeCell Color LCD Controller (PL111) Technical Reference Manual.

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "console.h"
#include "kernel.h"
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
#define   LCD_BPP16         (6U << 1)     //   16 bits per pixel
#define   LCD_PWR           (1U << 11)    //   LCD Power Enable

// Character buffer
static struct {
  char     ch;
  uint16_t fg;
  uint16_t bg;
} buf[BUF_SIZE];

static unsigned cur_pos;              // Cursor position

static uint16_t *frame_buf;           // Framebuffer base address
static volatile uint32_t *lcd;        // LCD registers base address
static uint8_t *font;                 // Font characters bitmap

// Create a 16-bit 5:6:5 RGB color reperesentation
#define RGB(r, g, b)  (((r) / 8) | (((g) / 4) << 5) | (((b) / 8) << 11))

// Map ANSI color codes to 16-bit colors
static uint16_t colors[] = {
  [COLOR_BLACK]          = RGB(0, 0, 0),
  [COLOR_RED]            = RGB(222, 56, 43),
  [COLOR_GREEN]          = RGB(0, 187, 0),
  [COLOR_YELLOW]         = RGB(255, 199, 6),
  [COLOR_BLUE]           = RGB(0, 111, 184),
  [COLOR_MAGENTA]        = RGB(118, 38, 113),
  [COLOR_CYAN]           = RGB(44, 181, 233),
  [COLOR_WHITE]          = RGB(187, 187, 187),
  [COLOR_GRAY]           = RGB(85, 85, 85),
  [COLOR_BRIGHT_RED]     = RGB(255, 0, 0),
  [COLOR_BRIGHT_GREEN]   = RGB(85, 255, 85),
  [COLOR_BRIGHT_YELLOW]  = RGB(255, 255, 85),
  [COLOR_BRIGHT_BLUE]    = RGB(0, 0, 255),
  [COLOR_BRIGHT_MAGENTA] = RGB(255, 0, 255),
  [COLOR_BRIGHT_CYAN]    = RGB(0, 255, 255),
  [COLOR_BRIGHT_WHITE]   = RGB(255, 255, 255),
};

// Display resolution, in pixels
#define DISPLAY_WIDTH     640
#define DISPLAY_HEIGHT    480
#define DISPLAY_SIZE      (DISPLAY_WIDTH * DISPLAY_HEIGHT)

// Single font character dimensions, in pixels
#define GLYPH_WIDTH       8
#define GLYPH_HEIGHT      16
#define GLYPH_SIZE        (GLYPH_WIDTH * GLYPH_HEIGHT)

static void lcd_buf_copy(unsigned, unsigned, size_t);
static void lcd_buf_fill(unsigned, size_t, uint16_t, uint16_t);
static void lcd_buf_putc(unsigned, char, uint16_t, uint16_t);

static void lcd_vid_copy(unsigned, unsigned, size_t);
static void lcd_vid_fill(unsigned, size_t, uint16_t);
static void lcd_vid_draw(unsigned, char, uint16_t, uint16_t);

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be equal to PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

/**
 * Initialize the LCD driver.
 */
int
lcd_init(void)
{
  extern uint8_t _binary_kernel_vga_font_psf_start[];

  struct PageInfo *p;
  struct PsfHeader *psf;

  // Load the font file.
  psf = (struct PsfHeader *) _binary_kernel_vga_font_psf_start;

  if ((psf->magic != PSF_MAGIC) || (psf->charsize != GLYPH_HEIGHT))
    return -EINVAL;

  font = (uint8_t *) (psf + 1);

  // Allocate the frame buffer.
  if ((p = page_alloc_block(8, PAGE_ALLOC_ZERO)) == NULL)
    return -ENOMEM;

  frame_buf = (uint16_t *) page2kva(p);
  p->ref_count++;

  lcd = (volatile uint32_t *) vm_map_mmio(LCD_BASE, 4096);

  // Display resolution: VGA (640x480) on VGA.
  lcd[LCD_TIMING0] = 0x3F1F3F9C;
  lcd[LCD_TIMING1] = 0x090B61DF;
  lcd[LCD_TIMING2] = 0x067F1800;

  // Frame buffer physical base address.
  lcd[LCD_UPBASE] = PADDR(frame_buf);

  // Enable LCD, 16 bpp.
  lcd[LCD_CONTROL] = LCD_EN | LCD_BPP16 | LCD_PWR;

  // Clear the screen
  lcd_buf_fill(0, BUF_SIZE, colors[COLOR_WHITE], colors[COLOR_BLACK]);

  return 0;
}

void
lcd_putc(unsigned pos, char c, int fg, int bg)
{
  lcd_buf_putc(pos, c, colors[fg], colors[bg]);
  lcd_vid_draw(pos, c, colors[fg], colors[bg]);
}

void
lcd_copy(unsigned to, unsigned from, size_t n)
{
  lcd_buf_copy(to, from, n);
  lcd_vid_copy(to, from, n);
}

void
lcd_fill(unsigned to, size_t n, int fg, int bg)
{
  lcd_buf_fill(to, n, colors[fg], colors[bg]);
  lcd_vid_fill(to, n, colors[bg]);
}

void
lcd_move_cursor(unsigned pos)
{
  // Draw the character at the old cursor position effectively erasing the
  // cursor.
  lcd_vid_draw(cur_pos, buf[cur_pos].ch, buf[cur_pos].fg, buf[cur_pos].bg);
  
  // Highlight the new cursor by inerting the foreground and the background
  // colors at the corresponding position.
  lcd_vid_draw(pos, buf[pos].ch, buf[pos].bg, buf[pos].fg);

  cur_pos = pos;
}


/*
 * ----------------------------------------------------------------------------
 * Text buffer operations
 * ----------------------------------------------------------------------------
 */

static void
lcd_buf_copy(unsigned to, unsigned from, size_t n)
{
  memcpy(&buf[to], &buf[from], n * sizeof(buf[0]));
}

static void
lcd_buf_fill(unsigned to, size_t n, uint16_t fg, uint16_t bg)
{
  while (n--)
    lcd_buf_putc(to++, ' ', fg, bg);
}

static void
lcd_buf_putc(unsigned i, char c, uint16_t fg, uint16_t bg)
{
  buf[i].ch = c;
  buf[i].fg = fg;
  buf[i].bg = bg;
}


/*
 * ----------------------------------------------------------------------------
 * Frame buffer
 * ----------------------------------------------------------------------------
 */

static void
lcd_vid_copy(unsigned to, unsigned from, size_t n)
{
  unsigned x, y;
  size_t i;
  uint16_t *src, *dst;

  for (i = 0; i < n; to++, from++, i++) {
    if (from == cur_pos) {
      lcd_vid_draw(to, buf[from].ch, buf[from].fg, buf[from].bg);
    } else {
      dst = &frame_buf[(to / BUF_WIDTH) * GLYPH_HEIGHT * DISPLAY_WIDTH];
      dst += (to % BUF_WIDTH) * GLYPH_WIDTH;

      src = &frame_buf[(from / BUF_WIDTH) * GLYPH_HEIGHT * DISPLAY_WIDTH];
      src += (from % BUF_WIDTH) * GLYPH_WIDTH;

      for (x = 0; x < GLYPH_WIDTH; x++)
        for (y = 0; y < GLYPH_HEIGHT; y++)
          dst[DISPLAY_WIDTH * y + x] = src[DISPLAY_WIDTH * y + x];
    }
  }
}

static void
lcd_vid_fill(unsigned to, size_t n, uint16_t color)
{
  unsigned x, y, x0, y0;
  size_t i;

  for (i = 0; i < n; to++, i++) {
    x0 = (to % BUF_WIDTH) * GLYPH_WIDTH;
    y0 = (to / BUF_WIDTH) * GLYPH_HEIGHT;

    for (x = 0; x < GLYPH_WIDTH; x++)
      for (y = 0; y < GLYPH_HEIGHT; y++)
        frame_buf[DISPLAY_WIDTH * (y0 + y) + (x0 + x)] = color;
  }
}

// Naive code to draw a character on the screen pixel-by-pixel. A more
// efficient solution would use boolean operations and a "mask lookup table"
// instead.
// TODO: implement this solution for better performance!
static void
lcd_vid_draw(unsigned pos, char c, uint16_t fg, uint16_t bg)
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
      frame_buf[DISPLAY_WIDTH * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? fg
        : bg;
}
