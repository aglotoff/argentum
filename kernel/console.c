#include <stdint.h>
#include <stdio.h>

#include "armv7.h"
#include "console.h"
#include "kmi.h"
#include "lcd.h"
#include "monitor.h"
#include "spinlock.h"
#include "uart.h"

#define CONSOLE_BUF_SIZE  256

// Circular input buffer
static struct {
  char            buf[CONSOLE_BUF_SIZE];
  uint32_t        rpos;
  uint32_t        wpos;
} input;

static struct {
  struct Spinlock lock;
  int             locking;
} console;

void
console_intr(int (*getc)(void))
{
  int c;

  if (console.locking)
    spin_lock(&console.lock);

  while ((c = getc()) >= 0) {
    if (c == 0) {
      continue;
    }

    if (input.wpos == input.rpos + CONSOLE_BUF_SIZE) {
      input.rpos++;
    }

    input.buf[input.wpos++ % CONSOLE_BUF_SIZE] = c;
    // console_putc(c);
  }

  if (console.locking)
    spin_unlock(&console.lock);
}

void
console_init(void)
{ 
  spin_init(&console.lock, "console");
  console.locking = 1;

  uart_init();
  kmi_kbd_init();
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
    kmi_kbd_intr();
  } while (input.rpos == input.wpos);

  return input.buf[input.rpos++ % CONSOLE_BUF_SIZE];
}

// Callback used by the vcprintf and cprintf functions.
static int
cputc(void *arg, int c)
{
  (void) arg;
  console_putc(c);
  return 1;
}

/*
 * ----------------------------------------------------------------------------
 * Formatted output
 * ----------------------------------------------------------------------------
 */

void
vcprintf(const char *format, va_list ap)
{
  if (console.locking)
    spin_lock(&console.lock);

  __printf(cputc, NULL, format, ap);
  
  if (console.locking)
    spin_unlock(&console.lock);
}

void
cprintf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vcprintf(format, ap);
  va_end(ap);
}

const char *panicstr;

void
__panic(const char *file, int line, const char *format, ...)
{
  va_list ap;

  if (!panicstr) {
    panicstr = format;
    console.locking = 0;

    cprintf("\x1b[1;31mkernel panic at %s:%d: ", file, line);

    va_start(ap, format);
    vcprintf(format, ap);
    va_end(ap);

    cprintf("\n\x1b[0m");
  }

  // Never returns.
  for (;;)
    monitor(NULL);
}

void
__warn(const char *file, int line, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  
  cprintf("kernel warning at %s:%d: ", file, line);
  vcprintf(format, ap);
  cprintf("\n");

  va_end(ap);
}
