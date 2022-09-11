#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <cprintf.h>
#include <cpu.h>
#include <drivers/console.h>
// #include <drivers/eth.h>
#include <drivers/gic.h>
#include <drivers/rtc.h>
#include <drivers/sd.h>
#include <fs/buf.h>
#include <fs/file.h>
#include <mm/kmem.h>
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
  kmem_init();  // Object allocator
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
