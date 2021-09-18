#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "armv7.h"
#include "console.h"
#include "gic.h"
#include "memlayout.h"
#include "monitor.h"
#include "mmu.h"
#include "page.h"
#include "sbcon.h"
#include "vm.h"

void
main(void)
{
  page_init_low();
  vm_init_kernel();
  page_init_high();
  gic_init();
  console_init();
  sb_init();

  cprintf("Starting CPU %d\n", read_mpidr() & 0x3);
  sb_rtc_time();

  // Enable interrupts
  // write_cpsr(read_cpsr() & ~PSR_I);

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

  // Higher-half mapping for the first 16MB of physical memory
  [TTX(KERNEL_BASE+0x00000000)] =
    (0x00000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00100000)] =
    (0x00100000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00200000)] =
    (0x00200000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00300000)] =
    (0x00300000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00400000)] =
    (0x00400000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00500000)] =
    (0x00500000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00600000)] =
    (0x00600000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00700000)] =
    (0x00700000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00800000)] =
    (0x00800000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00900000)] =
    (0x00900000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00A00000)] =
    (0x00A00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00B00000)] =
    (0x00B00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00C00000)] =
    (0x00C00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00D00000)] =
    (0x00D00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00E00000)] =
    (0x00E00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x00F00000)] =
    (0x00F00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),

  // Higher half mapping for I/O devices
  [TTX(KERNEL_BASE+0x10000000)] =
    (0x10000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE+0x1F000000)] =
    (0x1F000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
};
