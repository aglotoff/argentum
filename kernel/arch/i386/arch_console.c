#include <kernel/console.h>
#include <stdint.h>
#include <string.h>
#include <kernel/mm/memlayout.h>
#include <kernel/drivers/screen.h>
#include <kernel/tty.h>

extern struct Screen screen;

int
arch_console_getc(void)
{
  // TODO
  return -1;
}

void
arch_console_putc(char c)
{
  screen_out_char(tty_system->out.screen, c);
  screen_flush(tty_system->out.screen);
}
