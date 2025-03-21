#include <stdio.h>

#include <kernel/core/irq.h>

#include <kernel/console.h>
#include <kernel/tty.h>
#include <kernel/monitor.h>
#include <kernel/core/spinlock.h>

static struct KSpinLock lock = K_SPINLOCK_INITIALIZER("cprintf");
static int locking = 1;

/**
 * Return the next input character from the console. Polls for any pending
 * input characters.
 * 
 * @returns The next input character.
 */
int
console_getc(void)
{
  int c;
  
  // Poll for any pending characters from UART and the keyboard.
  while ((c = arch_console_getc()) <= 0)
    ;

  return c;
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  if (tty_system != NULL)
    arch_console_putc(c);
}

static int
cputc(void *arg, int c)
{
  (void) arg;
  console_putc(c);
  return 1;
}

int   __printf(int (*)(void *, int), void *, const char *, va_list);

/**
 * Printf-like formatted output to the console.
 * 
 * @param format The format string.
 * @param ap     A variable argument list.
 */
void
vcprintf(const char *format, va_list ap)
{
  if (locking)
    k_spinlock_acquire(&lock);

  __printf(cputc, NULL, format, ap);
  
  if (locking)
    k_spinlock_release(&lock);
}

/**
 * Printf-like formatted output to the console.
 * 
 * @param format The format string.
 */
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
_panic(const char *file, int line, const char *format, ...)
{
  va_list ap;

  k_irq_disable();

  if (!panicstr) {
    panicstr = format;
    locking = 0;

    cprintf("kernel panic at %s:%d: ", file, line);

    va_start(ap, format);
    vcprintf(format, ap);
    va_end(ap);

    cprintf("\n");

    // Never returns.
    while (1)
      monitor(NULL);
  } else {
    while (1)
      ;
  }
}

void
_warn(const char *file, int line, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  
  cprintf("kernel warning at %s:%d: ", file, line);
  vcprintf(format, ap);
  cprintf("\n");

  va_end(ap);
}
