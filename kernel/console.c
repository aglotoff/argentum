#include <stdint.h>
#include <stdio.h>

#include "console.h"
#include "kmi.h"
#include "lcd.h"
#include "uart.h"

#define CONSOLE_BUF_SIZE  256

// Circular input buffer
static struct {
  char      buf[CONSOLE_BUF_SIZE];
  uint32_t  rpos;
  uint32_t  wpos;
} console;

void
console_intr(int (*getc)(void))
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
    // Poll for any pending characters from UART and the keyboard.
    uart_intr();
    kmi_intr();
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
