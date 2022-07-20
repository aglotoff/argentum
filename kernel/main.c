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
#include <mm/page.h>
#include <mm/vm.h>
#include <process.h>
#include <sync.h>

static void mp_main(void);

// Whether the bootstrap processor has finished its initialization?
int bsp_started;

// For uname()
struct utsname utsname = {
  .sysname  = "OSDev-PBX-A9",
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
  // Setup the memory mappings first
  page_init_low();      // Physical page allocator (lower memory)
  kobject_pool_init();  // Object allocator
  vm_init();            // Kernel virtual memory

  // Now we can initialize the console and print messages
  gic_init();           // Interrupt controller
  console_init();       // Console driver

  // Perform the rest of initialization
  page_init_high();     // Physical page allocator (higher memory)
  
  ptimer_init();        // Private timer
  rtc_init();           // Real-time clock
  sd_init();            // MultiMedia Card Interface
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
  vm_init_percpu();     // Load the kernel translation table
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

// Initial translation table
__attribute__((__aligned__(TRTAB_SIZE))) tte_t
entry_trtab[NTTENTRIES] = {
  // Identity mapping for the first 1MB of physical memory (enough to load
  // the entry section):
  [0x000] =                (0x000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),

  // Higher-half mapping for the first 16MB of physical memory (enough to 
  // initialize the page allocator data structures, setup the master
  // translation table and allocate the LCD framebuffer):
  [TTX(KERNEL_BASE)+0x0] = (0x000000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x1] = (0x100000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x2] = (0x200000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x3] = (0x300000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x4] = (0x400000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x5] = (0x500000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x6] = (0x600000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x7] = (0x700000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x8] = (0x800000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0x9] = (0x900000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0xA] = (0xA00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0xB] = (0xB00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0xC] = (0xC00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0xD] = (0xD00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0xE] = (0xE00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
  [TTX(KERNEL_BASE)+0xF] = (0xF00000 | TTE_TYPE_SECT | TTE_SECT_AP(AP_PRIV_RW)),
};
