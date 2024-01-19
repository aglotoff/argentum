#include <kernel/assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/drivers/console.h>
#include <kernel/drivers/eth.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/file.h>
#include <kernel/irq.h>
#include <kernel/kqueue.h>
#include <kernel/ksemaphore.h>
#include <kernel/ktimer.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/vmspace.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/sd.h>
#include <kernel/net.h>

static void mp_main(void);

// Whether the bootstrap processor has finished its initialization?
int bsp_started;

// For uname()
struct utsname utsname = {
  .sysname = "OSDev",
  .nodename = "localhost",
  .release = "0.1.0",
  .version = __DATE__ " " __TIME__,
  .machine = "arm",
};

/**
 * Main kernel function.
 *
 * The bootstrap processor starts running C code here.
 */
void main(void)
{
  // Begin the memory manager initialization
  page_init_low(); // Physical page allocator (lower memory)
  vm_init();       // Memory management unit and kernel mappings

  // Now we can initialize the console to print messages during initialization
  irq_init();     // Interrupt controller
  console_init(); // Console driver

  // Complete the memory manager initialization
  page_init_high(); // Physical page allocator (higher memory)
  kmem_init();      // Object allocator
  vm_space_init();  // Virtual memory manager

  // Initialize the device drivers
  rtc_init(); // Real-time clock
  sd_init();  // MultiMedia Card
  eth_init(); // Ethernet

  // Initialize the remaining kernel services
  buf_init();     // Buffer cache
  file_init();    // File table
  sched_init();   // Scheduler
  net_init();     // Network
  pipe_init();
  process_init(); // Process table

  // Unblock other CPUs
  bsp_started = 1;

  mp_main();
}

/**
 * Initialization code for non-boot (AP) processors.
 *
 * AP processors jump here from 'entry.S'.
 */
void mp_enter(void)
{
  // Per-CPU initialization
  vm_init_percpu(); // Load the kernel page table
  irq_init_percpu();

  mp_main();
}

static void
mp_main(void)
{
  cprintf("Starting CPU %d\n", cpu_id());

  // Enter the scheduler loop
  sched_start();
}
