#include <assert.h>
#include <stddef.h>

#include "armv7.h"
#include "console.h"
#include "memlayout.h"
#include "monitor.h"
#include "mmu.h"

void
main(void)
{
  console_init();
  
  cprintf("Starting CPU %d\n", read_mpidr() & 0x3);

  // Cause a data abort
  *(volatile int *) 0xF0000000 = 12345;

  for (;;) {
    monitor(NULL);
  }
}

const char *panicstr;

void
__panic(const char *file, int line, const char *format, ...)
{
  va_list ap;

  if (!panicstr) {
    panicstr = format;

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

// Initial translation table that maps just enough physical memory to get us
// through the early boot process.
__attribute__((__aligned__(TRTAB_SIZE))) tte_t
entry_trtab[NTTENTRIES] = {
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
