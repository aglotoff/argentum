#include "console.h"

void
main(void)
{
  console_init();

  cprintf("Hello world!\n");

  for (;;) {
    console_putc(console_getc());
  }
}
