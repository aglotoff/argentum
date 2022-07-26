#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <armv7.h>
#include <cprintf.h>
#include <cpu.h>
#include <drivers/console.h>
// #include <drivers/eth.h>
#include <drivers/gic.h>
#include <drivers/rtc.h>
#include <drivers/sd.h>
#include <fs/buf.h>
#include <fs/file.h>
#include <mm/kobject.h>
#include <mm/memlayout.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/vm.h>
#include <process.h>
#include <sync.h>

static void mp_main(void);

// Whether the bootstrap processor has finished its initialization?
int bsp_started;

// For uname()
struct utsname utsname = {
  .sysname  = "Argentum",
  .nodename = "localhost",
  .release  = "0.1.0",
  .version  = __DATE__ " " __TIME__,
  .machine  = "arm",
};

/**
 * Main kernel function.
 * 
 * The bootstrap processor starts running C code here.
 */
void
main(void)
{
  // Begin the memory manager initialization
  page_init_low();      // Physical page allocator (lower memory)
  mmu_init();           // Memory management unit and kernel mappings

  // Now we can initialize the console to print messages during initialization
  gic_init();           // Interrupt controller
  console_init();       // Console driver

  // Complete the memory manager initialization
  page_init_high();     // Physical page allocator (higher memory)
  kobject_pool_init();  // Object allocator
  vm_init();            // Virtual memory manager

  // Initialize the rest of device drivers
  ptimer_init();        // Private timer
  rtc_init();           // Real-time clock
  sd_init();            // MultiMedia Card
  // eth_init();           // Ethernet

  // Initialize the remaining kernel services
  buf_init();           // Buffer cache
  file_init();          // File table
  scheduler_init();     // Scheduler
  process_init();       // Process table

  // Unblock other CPUs
  bsp_started = 1;    

  mp_main();
}

/**
 * Initialization code for non-boot (AP) processors. 
 *
 * AP processors jump here from 'entry.S'.
 */
void
mp_enter(void)
{
  // Per-CPU initialization
  mmu_init_percpu();    // Load the kernel page table
  gic_init();           // Interrupt controller
  ptimer_init();        // Private timer

  mp_main();
}

static void
mp_main(void)
{
  cprintf("Starting CPU %d\n", cpu_id());

  // Enter the scheduler loop
  scheduler_start();   
}

#define MAKE_L1_SECTION(pa, ap) \
  ((pa) | L1_DESC_TYPE_SECT | L1_DESC_SECT_AP(ap))

// Initial translation table
__attribute__((__aligned__(L1_TABLE_SIZE))) l1_desc_t
entry_trtab[L1_NR_ENTRIES] = {
  // Identity mapping for the first 1MB of physical memory (just enough to
  // load the entry point code):
  [0x0]                       = MAKE_L1_SECTION(0x000000, AP_PRIV_RW),

  // Higher-half mapping for the first 16MB of physical memory (should be
  // enough to initialize the page allocator data structures, setup the master
  // translation table and allocate the LCD framebuffer):
  [L1_IDX(KERNEL_BASE) + 0x0] = MAKE_L1_SECTION(0x000000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x1] = MAKE_L1_SECTION(0x100000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x2] = MAKE_L1_SECTION(0x200000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x3] = MAKE_L1_SECTION(0x300000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x4] = MAKE_L1_SECTION(0x400000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x5] = MAKE_L1_SECTION(0x500000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x6] = MAKE_L1_SECTION(0x600000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x7] = MAKE_L1_SECTION(0x700000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x8] = MAKE_L1_SECTION(0x800000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0x9] = MAKE_L1_SECTION(0x900000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0xA] = MAKE_L1_SECTION(0xA00000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0xB] = MAKE_L1_SECTION(0xB00000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0xC] = MAKE_L1_SECTION(0xC00000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0xD] = MAKE_L1_SECTION(0xD00000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0xE] = MAKE_L1_SECTION(0xE00000, AP_PRIV_RW),
  [L1_IDX(KERNEL_BASE) + 0xF] = MAKE_L1_SECTION(0xF00000, AP_PRIV_RW),
};
