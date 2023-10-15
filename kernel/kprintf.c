#include <stdio.h>

#include <arch_console.h>
#include <kernel.h>
#include <spinlock.h>

static struct SpinLock kprintf_lock = SPIN_INITIALIZER("console");

static void
kputc(void *unused, int c)
{
  (void) unused;
  arch_console_putc(c);
}

/**
 * Print formatted data from variable argument list to the console.
 * 
 * @param format The format string
 * @param ap     The variable argument list
 */
void
vkprintf(const char *format, va_list ap)
{
  spin_lock(&kprintf_lock);
  xprintf(kputc, NULL, format, ap);
  spin_unlock(&kprintf_lock);
}

/**
 * Print formatted data to the console.
 * 
 * @param format The format string
 */
void
kprintf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vkprintf(format, ap);
  va_end(ap);
}
