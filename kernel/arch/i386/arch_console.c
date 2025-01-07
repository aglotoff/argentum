#include <kernel/console.h>
#include <stdint.h>
#include <string.h>
#include <kernel/mm/memlayout.h>
#include <kernel/drivers/screen.h>
#include <kernel/tty.h>
#include <arch/i386/i8042.h>
#include <arch/i386/rs232.h>

extern struct Screen screen;
extern struct I8042 i8042;

int
arch_console_getc(void)
{
  int c;
  
  if ((c = i8042_getc(&i8042)) > 0)
    return c;

  return rs232_getc();
}

void
arch_console_putc(char c)
{
  rs232_putc(c);

  screen_out_char(tty_system->out.screen, c);
  screen_flush(tty_system->out.screen);
}
