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

#define DISPLAY_WIDTH     640
#define DISPLAY_HEIGHT    480

#define GLYPH_WIDTH       8
#define GLYPH_HEIGHT      16

#define TEXT_COLS         (DISPLAY_WIDTH / GLYPH_WIDTH)
#define TEXT_ROWS         (DISPLAY_HEIGHT / GLYPH_HEIGHT)
#define TEXT_SIZE         (TEXT_COLS * TEXT_ROWS)

// 15-bit RGB ANSI colors
#define BLACK             0x0000
#define RED               0x0019
#define GREEN             0x0320
#define YELLOW            0x0339
#define BLUE              0x7400
#define MAGENTA           0x6419
#define CYAN              0x6720
#define WHITE             0x739c
#define GRAY              0x3def
#define BRIGHT_RED        0x001f
#define BRIGHT_GREEN      0x03e0
#define BRIGHT_YELLOW     0x03ff
#define BRIGHT_BLUE       0x7d6b
#define BRIGHT_MAGENTA    0x7c1f
#define BRIGHT_CYAN       0x7fe0
#define BRIGHT_WHITE      0x7fff

static void lcd_draw_char(char, uint32_t, uint16_t, uint16_t);
static void lcd_flush(void);
static void lcd_update_cursor(void);
static void lcd_scroll_screen(void);

// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
struct PsfHeader {
  uint16_t  magic;          // Must be PSF_MAGIC
	uint8_t   mode;	          // PSF font mode
	uint8_t   charsize;	      // Character size
} __attribute__((__packed__));

#define PSF_MAGIC  0x0436

// Address of the embedded PSF font file
extern uint8_t _binary_kernel_vga_font_psf_start[];
// Address of the bitmap to draw characters on the screen
static uint8_t *font;

static char text_buf[TEXT_SIZE];
static uint16_t text_pos, text_cur;

// Framebuffer
static uint16_t *buf;
static volatile uint32_t *lcd;

/**
 * Initialize the LCD driver.
 */
void
lcd_init(void)
{
  struct PageInfo *p;
  struct PsfHeader *psf_header;
  
  psf_header = (struct PsfHeader *) _binary_kernel_vga_font_psf_start;

  // Make sure the font file is valid.
  if (psf_header->magic != PSF_MAGIC || psf_header->charsize != GLYPH_HEIGHT)
    return;

  font = (uint8_t *) (psf_header + 1);

  // Allocate the frame buffer.
  if ((p = page_alloc_block(8, PAGE_ALLOC_ZERO)) == NULL)
    return;

  buf = (uint16_t *) page2kva(p);
  p->ref_count++;

  lcd = (volatile uint32_t *) vm_map_mmio(LCD_BASE, 4096);

  // Set display resolution: VGA (640x480) on VGA.
  lcd[LCD_TIMING0] = 0x3F1F3F9C;
  lcd[LCD_TIMING1] = 0x090B61DF;
  lcd[LCD_TIMING2] = 0x067F1800;

  // Set the frame buffer physical base address.
  lcd[LCD_UPBASE] = PADDR(buf);

  // Enable LCD, 16 bpp.
  lcd[LCD_CONTROL] = LCD_EN | LCD_BPP16 | LCD_PWR;
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

  switch (c) {
  case '\n':
    text_pos += TEXT_COLS;
    // fall through
  case '\r':
    text_pos -= text_pos % TEXT_COLS;
    break;

  case '\b':
    if (text_pos > 0) {
      text_buf[--text_pos] = ' ';
      lcd_flush();
    }
    break;

  case '\t':
    do {
      text_buf[text_pos++] = ' ';
    } while ((text_pos % 4) != 0);
    lcd_flush();
    break;

  default:
    text_buf[text_pos++] = c;
    lcd_flush();
    break;
  }

  if (text_pos >= TEXT_SIZE)
    lcd_scroll_screen();

  lcd_update_cursor();
}

// Naive code to draw a character on the screen pixel-by-pixel. A more efficient
// solution would use boolean operations and a "mask lookup table" instead.
// TODO: implement this solution for better performance!
static void
lcd_draw_char(char c, uint32_t pos, uint16_t fg, uint16_t bg)
{
  static uint8_t mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };

  uint8_t *glyph;
  uint16_t x0, y0, x, y;

  if (c == '\0')
    c = ' ';
  glyph = &font[c * GLYPH_HEIGHT];

  x0 = (pos % TEXT_COLS) * GLYPH_WIDTH;
  y0 = (pos / TEXT_COLS) * GLYPH_HEIGHT;

  for (x = 0; x < GLYPH_WIDTH; x++)
    for (y = 0; y < GLYPH_HEIGHT; y++)
      buf[DISPLAY_WIDTH * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x]) ? fg : bg;
}

static void
lcd_flush(void)
{
  uint16_t i;

  i = text_cur;

  while (i > text_pos) {
    i--;
    lcd_draw_char(text_buf[i], i, WHITE, BLACK);
  }

  while (i < text_pos) {
    lcd_draw_char(text_buf[i], i, WHITE, BLACK);
    i++;
  }
}

static void
lcd_update_cursor(void)
{
  lcd_draw_char(text_buf[text_cur], text_cur, WHITE, BLACK);
  lcd_draw_char(text_buf[text_pos], text_pos, BLACK, WHITE);

  text_cur = text_pos;
}

static void
lcd_scroll_screen(void)
{
  memcpy(text_buf, &text_buf[TEXT_COLS],
          (TEXT_SIZE - TEXT_COLS) * sizeof(text_buf[0]));
  memset(&text_buf[TEXT_SIZE - TEXT_COLS], 0,
          TEXT_COLS * sizeof(text_buf[0]));
  
  memcpy(buf, &buf[GLYPH_HEIGHT * DISPLAY_WIDTH],
          DISPLAY_WIDTH * (DISPLAY_HEIGHT - GLYPH_HEIGHT) * sizeof(buf[0]));
  memset(&buf[DISPLAY_WIDTH * (DISPLAY_HEIGHT - GLYPH_HEIGHT)], 0,
        GLYPH_HEIGHT * DISPLAY_WIDTH * sizeof(buf[0]));

  text_pos -= TEXT_COLS;
  text_cur -= TEXT_COLS;
}
