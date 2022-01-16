#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "armv7.h"
#include "buf.h"
#include "console.h"
#include "gic.h"
#include "kobject.h"
#include "memlayout.h"
#include "mmu.h"
#include "page.h"
#include "process.h"
#include "rtc.h"
#include "sd.h"
#include "vm.h"

static void boot_aps(void);
static void mp_main(void);

/**
 * Main kernel function.
 * 
 * The bootstrap processor starts running C code here.
 */
void
main(void)
{
  console_init();       // Console driver
  cprintf("Starting CPU %d\n", read_mpidr() & 0x3);
  
  page_init_low();      // Physical page allocator (lower memory)
  vm_init();            // Kernel virtual memory
  page_init_high();     // Physical page allocator (higher memory)
  gic_init();           // Interrupt controller
  kobject_pool_init();  // Object allocator
  process_init();       // Process table
  rtc_init();           // Real-time clock
  sd_init();            // MultiMedia Card Interface
  buf_init();           // Buffer cache

#if defined(PROCESS_NAME)
  PROCESS_CREATE(PROCESS_NAME);
#else
  PROCESS_CREATE(hello);
#endif

  boot_aps();           // Start other CPUs
  mp_main();            // Finish the processor's setup
}

/**
 * Initialization code for non-boot (AP) processors. 
 *
 * AP processors jump here from entry.S.
 */
void
mp_enter(void)
{
  vm_init_percpu();

  cprintf("Starting CPU %d\n", read_mpidr() & 0x3);

  mp_main();
}

#define SYS_BASE      0x10000000
#define SYS_FLAGS     (0x030 >> 2)

static void
boot_aps(void)
{
  extern uint8_t _start[];
  static volatile uint32_t *sys;

  sys = (volatile uint32_t *) KADDR(SYS_BASE);
  sys[SYS_FLAGS] = (uint32_t) _start;

  gic_start_others();
}

// Common CPU setup code
static void
mp_main(void)
{
  ptimer_init();

  scheduler();
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

  // Map console devices registers
  [TTX(KERNEL_BASE+0x10000000)] =
    (0x10000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
};
