#include <string.h>


#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/interrupt.h>
#include <kernel/console.h>
#include <kernel/tty.h>

void kernel_main(void) 
{
	for (;;);
}

void main(void);
void mp_main(void);

void
arch_init(void)
{
	page_init_low();
	arch_vm_init();
	page_init_high();

	tty_init();

  for (int i = 0; i < 40; i++)
    cprintf("Hello World  %d\n", i);

  kernel_main();
}

void
arch_init_devices(void)
{
  // TODO
}


void
arch_mp_init(void)
{
  // TODO
}

void
arch_eth_write(const void *buf, size_t n)
{
  // TODO
  (void) buf;
  (void) n;
}
