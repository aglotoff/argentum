#include <kernel/tty.h>
#include <kernel/drivers/screen.h>

#include <arch/i386/vga.h>
#include <arch/i386/i8042.h>

#define NSCREENS    6   // For now, all ttys are screens

static struct Screen screens[NSCREENS];

struct Vga vga;
struct I8042 i8042;

void
arch_tty_init_system(void)
{
  i8042_init(&i8042, 0);
  vga_init(&vga, (void *) 0x800B8000, &screens[0]);
}

void
arch_tty_init(struct Tty *tty, int i)
{
  tty->out.screen = &screens[i];
  screen_init(tty->out.screen, &vga_ops, &vga, 80, 25);
}

void
arch_tty_switch(struct Tty *tty)
{
  screen_switch(tty->out.screen);
}

void
arch_tty_out_char(struct Tty *tty, char c)
{
  screen_out_char(tty->out.screen, c);
}

void
arch_tty_flush(struct Tty *tty)
{
  screen_flush(tty->out.screen);
}

void
arch_tty_erase(struct Tty *tty)
{
  screen_backspace(tty->out.screen);
}
