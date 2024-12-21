#include <kernel/tty.h>
#include <arch/arm/mach.h>

void
arch_tty_init_system(void)
{
  mach_current->tty_init_system();
}

void
arch_tty_init(struct Tty *tty, int i)
{
  mach_current->tty_init(tty, i);
}

void
arch_tty_switch(struct Tty *tty)
{
  mach_current->tty_switch(tty);
}

void
arch_tty_out_char(struct Tty *tty, char c)
{
  mach_current->tty_out_char(tty, c);
}

void
arch_tty_flush(struct Tty *tty)
{
  mach_current->tty_flush(tty);
}

void
arch_tty_erase(struct Tty *tty)
{
  mach_current->tty_erase(tty);
}
