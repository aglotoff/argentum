#include <kernel/console.h>
#include <arch/arm/mach.h>

int
arch_console_getc(void)
{
  return mach_current->console_getc();
}

void
arch_console_putc(char c)
{
  mach_current->console_putc(c);
}
