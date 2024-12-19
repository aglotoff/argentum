#include <kernel/assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <kernel/console.h>
#include <kernel/core/cpu.h>
#include <kernel/tty.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/file.h>
#include <kernel/core/irq.h>
#include <kernel/core/mailbox.h>
#include <kernel/mutex.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/timer.h>
#include <kernel/object_pool.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/vmspace.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/ipc.h>
#include <kernel/net.h>
#include <kernel/mach.h>
#include <kernel/interrupt.h>
#include <kernel/time.h>

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
  // Initialize the memory manager
  page_init_low();  // Physical page allocator (lower memory)
  arch_vm_init();   // Memory management unit and kernel mappings
  page_init_high(); // Physical page allocator (higher memory)

  // Initialize the machine
  mach_init(mach_type);

  // Initialize the interrupt controller
  arch_interrupt_init();

  // Initialize core services
  k_object_pool_system_init();
  k_mutex_system_init();
  k_semaphore_system_init();
  k_mailbox_system_init();
  k_sched_init();

  // Initialize device drivers
  tty_init();                   // Console
  mach_current->storage_init();
  mach_current->eth_init();

  // Initialize the remaining kernel services
  buf_init();           // Buffer cache
  file_init();          // File table
  vm_space_init();      // Virtual memory manager
  pipe_init();          // Pipes
  process_init();       // Process table
  net_init();           // Networking

  // ipc_init();

  time_init();

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
  arch_vm_init_percpu(); // Load the kernel page table
  arch_interrupt_init_percpu();

  mp_main();
}

static void
mp_main(void)
{
  cprintf("Starting CPU %d\n", k_cpu_id());

  // Enter the scheduler loop
  k_sched_start();
}
