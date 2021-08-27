#include <stdint.h>

#include "console.h"
#include "memlayout.h"
#include "mmu.h"

void
main(int a)
{
  console_init();

  cprintf("Hello world %p!\n", &a);

  for (;;) {
    console_putc(console_getc());
  }
}

// Initial translation table that maps just enough physical memory to get us
// through the early boot process.
__attribute__((__aligned__(TRTAB_SIZE))) tte_t
boot_trtab[NTTENTRIES] = {
  // Identity mapping for the first 1MB of physical memory
  [TTX(0x00000000)] =
    (0x00000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),

  // Higher-half mapping for the first 1MB of physical memory
  [TTX(KERNEL_BASE+0x00000000)] =
    (0x00000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),

  // Higher half mapping for console I/O devices
  [TTX(KERNEL_BASE+0x10000000)] =
    (0x10000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
};
