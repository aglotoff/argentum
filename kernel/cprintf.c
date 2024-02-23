#include <stdio.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/console.h>
#include <kernel/monitor.h>
#include <kernel/spin.h>

static struct SpinLock lock = SPIN_INITIALIZER("cprintf");
static int locking = 1;

const char *panicstr;

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
    spin_lock(&lock);

  __printf(cputc, NULL, format, ap);
  
  if (locking)
    spin_unlock(&lock);
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

void
__panic(const char *file, int line, const char *format, ...)
{
  va_list ap;

  if (!panicstr) {
    panicstr = format;
    locking = 0;

    cprintf("kernel panic at %s:%d: ", file, line);

    va_start(ap, format);
    vcprintf(format, ap);
    va_end(ap);

    cprintf("\n");
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
