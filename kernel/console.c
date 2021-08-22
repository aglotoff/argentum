#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "console.h"
#include "kbd.h"
#include "lcd.h"

static void console_interrupt(int (*)(void));

/*
 * UART input/output code.
 *
 * See PrimeCell UART (PL011) Technical Reference Manual
 */

#define UART0             0x10009000    // UART0 memory base address

#define UARTDR            (0x000 >> 2)  // Data Register
#define UARTECR           (0x004 >> 2)  // Error Clear Register
#define UARTFR            (0x018 >> 2)  // Flag Register
#define   UARTFR_RXFE     (1 << 4)      // Receive FIFO empty
#define   UARTFR_TXFF     (1 << 5)      // Transmit FIFO full
#define UARTIBRD          (0x024 >> 2)  // Integer Baud Rate Register
#define UARTFBRD          (0x028 >> 2)  // Fractional Baud Rate Register
#define UARTLCR           (0x02C >> 2)  // Line Control Register
#define   UARTLCR_FEN     (1 << 4)      // Enable FIFOs
#define   UARTLCR_WLEN8   (3 << 5)      // Word length = 8 bits
#define UARTCR            (0x030 >> 2)  // Control Register
#define   UARTCR_UARTEN   (1 << 0)      // UART Enable
#define   UARTCR_TXE      (1 << 8)      // Transmit enable
#define   UARTCR_RXE      (1 << 9)      // Receive enable

#define UART_CLK          24000000      // Clock rate
#define UART_BAUDRATE     19200         // Baud rate

static volatile uint32_t *uart;

static void
uart_init(void)
{
  uint32_t divisor_x64;

  uart = (volatile uint32_t *) UART0;

  // Clear all errors
  uart[UARTECR] = 0;

  // Disable UART
  uart[UARTCR] = 0;

  // Set the bit rate
  divisor_x64 = (UART_CLK * 4) / UART_BAUDRATE;
  uart[UARTIBRD] = (divisor_x64 >> 6) & 0xFFFF;
  uart[UARTFBRD] = divisor_x64 & 0x3F;

  // Enable FIFO, 8 data bits, 1 stop bit, parity off
  uart[UARTLCR] = UARTLCR_FEN | UARTLCR_WLEN8;

  // Enable UART, transfer & receive
  uart[UARTCR] = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
}

static void
uart_putc(char c)
{
  while (uart[UARTFR] & UARTFR_TXFF)
    ;
  uart[UARTDR] = c;
}

static int
uart_getc(void)
{
  if (uart[UARTFR] & UARTFR_RXFE)
    return -1;
  return uart[UARTDR] & 0xFF;
}

static void
uart_interrupt(void)
{
  console_interrupt(uart_getc);
}


/*
 * Keyboard input code.
 * 
 * See ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050) Technical Reference
 * Manual.
 */

#define KMI0              0x10006000    // KMI0 (keyboard) base address

#define KMICR             (0x00 >> 2)   // Control register
#define KMISTAT           (0x04 >> 2)   // Status register
#define   KMISTAT_RXFULL  (1 << 4)      // Receiver register full
#define KMIDATA           (0x08 >> 2)   // Received data

static volatile uint32_t *kmi;
static int kmi_state;

static int
kmi_getc(void)
{
  int c;
  uint8_t data;

  if (!(kmi[KMISTAT] & KMISTAT_RXFULL))
    return -1;

  data = kmi[KMIDATA];

  if (data == 0xE0) {
    kmi_state |= E0ESC;
    return 0;
  }
  
  if (data & 0x80) {
    // Key released
    data = (kmi_state & E0ESC ? data : data & 0x7F);
    kmi_state &= ~(shiftcode[data] | E0ESC);
    return 0;
  }

  if (kmi_state & E0ESC) {
    data |= 0x80;
    kmi_state &= ~E0ESC;
  }

  kmi_state |= shiftcode[data];
  kmi_state ^= togglecode[data];

  c = charcode[kmi_state & (CTL | SHIFT)][data];
  if (kmi_state & CAPSLOCK) {
    if (c >= 'a' && c <= 'z') {
      c += 'A' - 'a';
    } else if (c >= 'A' && c <= 'Z') {
      c += 'a' - 'A';
    }
  }

  return c;
}

static void
kmi_init(void)
{
  kmi = (volatile uint32_t *) KMI0;
}

static void
kmi_interrupt(void)
{
  console_interrupt(kmi_getc);
}


/**
 * LCD output code.
 * 
 * See PrimeCell Color LCD Controller (PL111) Technical Reference Manual
 */

#define LCD             0x10020000      // LCD base address

#define LCD_TIMING0     (0x000 >> 2)    // Horizontal Axis Panel Control
#define LCD_TIMING1     (0x004 >> 2)    // Vertical Axis Panel Control
#define LCD_TIMING2     (0x008 >> 2)    // Clock and Signal Polarity Control
#define LCD_UPBASE      (0x010 >> 2)    // Upper Panel Frame Base Address
#define LCD_CONTROL     (0x018 >> 2)    // LCD Control
  #define LCD_EN        (1 << 0)        // CLCDC Enable
  #define LCD_BPP16     (4 << 1)        // 16 bits per pixel
  #define LCD_PWR       (1 << 11)       // LCD Power Enable

static volatile uint32_t *lcd;

static uint16_t buf[DISPLAY_WIDTH * DISPLAY_HEIGHT];
static uint16_t caret_x, caret_y;

static void
lcd_init(void)
{
  lcd = (volatile uint32_t *) LCD;

  // Display resolution: VGA (640x480) on VGA
  lcd[LCD_TIMING0] = 0x3F1F3F9C;
  lcd[LCD_TIMING1] = 0x090B61DF;
  lcd[LCD_TIMING2] = 0x067F1800;

  // Set frame buffer base address
  lcd[LCD_UPBASE] = (uint32_t) buf;

  // Enable LCD, 16 bpp
  lcd[LCD_CONTROL] = LCD_EN | LCD_BPP16 | LCD_PWR;
}

static void
lcd_draw_char(char c, uint16_t caret_x, uint16_t caret_y)
{
  uint8_t *glyph;
  uint16_t x, y, x0, y0;

  if (c < FONT_CHAR_MIN || c > FONT_CHAR_MAX) {
    // Display '?' for non-printable characters
    c = '?';
  }

  glyph = font8x16[c - FONT_CHAR_MIN];

  x0 = caret_x * FONT_CHAR_WIDTH;
  y0 = caret_y * FONT_LINE_HEIGHT;

  // Erase the previous character
  for (x = 0; x < FONT_CHAR_WIDTH; x++) {
    for (y = 0; y < FONT_LINE_HEIGHT; y++) {
      buf[DISPLAY_WIDTH * (y + y0) + (x + x0)] = 0x0000;
    }
  }

  // Draw the new character
  for (x = 0; x < FONT_CHAR_WIDTH; x++) {
    for (y = 0; y < FONT_CHAR_HEIGHT; y++) {
      if (glyph[x + (y / FONT_CHAR_WIDTH) * FONT_CHAR_WIDTH] & (1 << (y%8))) {
        buf[DISPLAY_WIDTH * (y + y0 + 2) + (x + x0)] = 0xFFFF;
      }
    }
  }
}

static void
lcd_putc(char c)
{
  // Erase the previous caret
  lcd_draw_char(' ', caret_x, caret_y);

  switch (c) {
  case '\n':
    caret_x = 0;
    caret_y++;
    break;
  case '\b':
    if (caret_x == 0 && caret_y > 0) {
      caret_x = COLS - 1;
      caret_y--;
    } else {
      caret_x--;
    }
    break;
  case '\t':
    lcd_putc(' ');
    lcd_putc(' ');
    lcd_putc(' ');
    lcd_putc(' ');
    break;
  default:
    lcd_draw_char(c, caret_x++, caret_y);
    if (caret_x == COLS) {
      caret_x = 0;
      caret_y++;
    }
  }

  if (caret_y >= ROWS) {
    // Scroll down
    memmove(buf,
            &buf[FONT_LINE_HEIGHT * DISPLAY_WIDTH],
            DISPLAY_WIDTH * (DISPLAY_HEIGHT-FONT_LINE_HEIGHT) * sizeof(buf[0]));
    memset(&buf[DISPLAY_WIDTH * (DISPLAY_HEIGHT-FONT_LINE_HEIGHT)],
          0,
          FONT_LINE_HEIGHT * DISPLAY_WIDTH * sizeof(buf[0]));

    caret_y--;
  }

  // Draw the new caret
  lcd_draw_char('_', caret_x, caret_y);
}


/*
 * General device-independent console code.
 */

#define CONSOLE_BUF_SIZE  256

// Circular input buffer
static struct {
  char      buf[CONSOLE_BUF_SIZE];
  uint32_t  rpos;
  uint32_t  wpos;
} console;

static void
console_interrupt(int (*getc)(void))
{
  int c;

  while ((c = getc()) >= 0) {
    if (c == 0) {
      continue;
    }
    if (console.wpos == console.rpos + CONSOLE_BUF_SIZE) {
      console.rpos++;
    }
    console.buf[console.wpos++ % CONSOLE_BUF_SIZE] = c;
  }
}

void
console_init(void)
{
  uart_init();
  kmi_init();
  lcd_init();
}

void
console_putc(char c)
{
  uart_putc(c);
  lcd_putc(c);
}

int
console_getc(void)
{
  do {
    uart_interrupt();
    kmi_interrupt();
  } while (console.rpos == console.wpos);

  return console.buf[console.rpos++ % CONSOLE_BUF_SIZE];
}

// Callback used by the vcprintf and cprintf functions.
static void
cputc(void *arg, int c)
{
  (void) arg;
  console_putc(c);
}

void
vcprintf(const char *format, va_list ap)
{
  xprintf(cputc, NULL, format, ap);
}

void
cprintf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vcprintf(format, ap);
  va_end(ap);
}
