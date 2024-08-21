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
#include <kernel/mailbox.h>
#include <kernel/mutex.h>
#include <kernel/semaphore.h>
#include <kernel/timer.h>
#include <kernel/object_pool.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/vmspace.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/sd.h>
#include <kernel/mailbox.h>
#include <kernel/net.h>
#include <kernel/semaphore.h>
#include <kernel/mach.h>

static void mp_main(void);

// Whether the bootstrap processor has finished its initialization?
int bsp_started;

// For uname()
struct utsname utsname = {
  .sysname = "Argentum",
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
void main(uintptr_t mach_type)
{
  mach_init(mach_type);

  // Initialize the memory manager
  page_init_low();  // Physical page allocator (lower memory)
  vm_arch_init();   // Memory management unit and kernel mappings
  page_init_high(); // Physical page allocator (higher memory)

  // Initialize core services
  k_object_pool_system_init();
  k_mutex_system_init();
  k_semaphore_system_init();
  k_mailbox_system_init();
  k_sched_init();

  vm_space_init();      // Virtual memory manager
  interrupt_init();     // Interrupt controller

  // Initialize the device drivers
  console_init();       // Console
  rtc_init();           // Real-time clock
  sd_init();            // MultiMedia Card
  eth_init();           // Ethernet

  // Initialize the remaining kernel services
  buf_init();           // Buffer cache
  file_init();          // File table
  pipe_init();          // Pipes
  process_init();       // Process table
  signal_init_system(); // Signals
  net_init();           // Networking

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
  vm_arch_init_percpu(); // Load the kernel page table
  interrupt_init_percpu();

  mp_main();
}

static void
mp_main(void)
{
  cprintf("Starting CPU %d\n", k_cpu_id());

  // Enter the scheduler loop
  k_sched_start();
}
