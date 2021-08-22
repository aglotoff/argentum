#include <stdint.h>
#include <string.h>

#include "lcd.h"

#define DISPLAY_WIDTH       640
#define DISPLAY_HEIGHT      480

#define GLYPH_WIDTH         8
#define GLYPH_HEIGHT        16

static volatile uint32_t *lcd;

// Framebuffer
static uint16_t buf[DISPLAY_WIDTH * DISPLAY_HEIGHT];
// Current character position in the framebuffer
static uint16_t buf_x, buf_y;

// Address of the embedded PSF font file
extern uint8_t _binary_kernel_vga_font_psf_start[];
// Address of the bitmap to draw characters on the screen
static uint8_t *font;

void
lcd_init(void)
{
  struct PsfHeader *psf_header;
  
  psf_header = (struct PsfHeader *) _binary_kernel_vga_font_psf_start;

  // Make sure the font file is valid.
  if (psf_header->magic != PSF_MAGIC || psf_header->charsize != GLYPH_HEIGHT) {
    return;
  }

  font = (uint8_t *) (psf_header + 1);

  // TODO: map to a reserved region in the virtual address space.
  lcd = (volatile uint32_t *) LCD;

  // Set display resolution: VGA (640x480) on VGA
  lcd[LCD_TIMING0] = 0x3F1F3F9C;
  lcd[LCD_TIMING1] = 0x090B61DF;
  lcd[LCD_TIMING2] = 0x067F1800;

  // Set the frame buffer base address
  lcd[LCD_UPBASE] = (uint32_t) buf;

  // Enable LCD, 16 bpp
  lcd[LCD_CONTROL] = LCD_EN | LCD_BPP16 | LCD_PWR;
}

// Naive code to draw a character on the screen pixel-by-pixel. A more efecient
// solution would use boolean operations and a "mask lookup table" instead.
// TODO: implement this solution for better performance!
static void
lcd_draw_char(char c, uint16_t x0, uint16_t y0)
{
  static uint8_t mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };

  uint8_t *glyph;
  uint16_t x, y;

  glyph = &font[c * GLYPH_HEIGHT];

  for (x = 0; x < GLYPH_WIDTH; x++) {
    for (y = 0; y < GLYPH_HEIGHT; y++) {
      buf[DISPLAY_WIDTH * (y0 + y) + (x0 + x)] = (glyph[y] & mask[x])
        ? 0xFFFF    // white
        : 0x0000;   // on black
    }
  }
}

void
lcd_putc(char c)
{
  if (!lcd)
    return;

  // Erase the previous caret
  lcd_draw_char(' ', buf_x, buf_y);

  switch (c) {
  case '\n':
    buf_x = 0;
    buf_y += GLYPH_HEIGHT;
    break;

  case '\b':
    if (buf_x == 0 && buf_y > 0) {
      buf_x = DISPLAY_WIDTH - GLYPH_WIDTH;
      buf_y -= GLYPH_HEIGHT;
    } else {
      buf_x -= GLYPH_WIDTH;
    }
    break;

  case '\t':
    lcd_putc(' ');
    lcd_putc(' ');
    lcd_putc(' ');
    lcd_putc(' ');
    break;

  default:
    lcd_draw_char(c, buf_x++, buf_y);
    
    buf_x += GLYPH_WIDTH;
    if (buf_x == DISPLAY_WIDTH) {
      buf_x = 0;
      buf_y += GLYPH_HEIGHT;
    }
  }

  if (buf_y >= DISPLAY_HEIGHT) {
    // Scroll one line down
    memmove(buf, &buf[GLYPH_HEIGHT * DISPLAY_WIDTH],
            DISPLAY_WIDTH * (DISPLAY_HEIGHT - GLYPH_HEIGHT) * sizeof(buf[0]));
    memset(&buf[DISPLAY_WIDTH * (DISPLAY_HEIGHT - GLYPH_HEIGHT)], 0,
          GLYPH_HEIGHT * DISPLAY_WIDTH * sizeof(buf[0]));

    buf_y -= GLYPH_HEIGHT;
  }

  // Draw the new caret
  lcd_draw_char('_', buf_x, buf_y);
}
