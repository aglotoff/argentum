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

char test_stacks[3][PAGE_SIZE];
struct KThread *test1, *test2, *test3;

struct KMutex *test_mux;

void
test_func1(void *unused)
{
  (void) unused;

  cprintf("1: Starting\n");
  cprintf("1: Acquiring mutex\n");

  k_mutex_lock(test_mux);

  cprintf("1: Mutex acquired\n");

  k_thread_yield();

  cprintf("1: Releasing mutex\n");

  k_mutex_unlock(test_mux);

  cprintf("1: Mutex released\n");

  k_thread_exit();
}

void
test_func2(void *unused)
{
  (void) unused;

  cprintf("2: Starting\n");
  k_thread_resume(test1);

  for (;;) {
    k_thread_yield();
  }
}

void
test_func3(void *unused)
{
  (void) unused;

  cprintf("3: Starting\n");
  cprintf("3: Acquiring mutex\n");

  k_mutex_lock(test_mux);

  cprintf("3: Mutex acquired\n");

  k_thread_resume(test2);
  k_thread_yield();

  cprintf("3: Releasing mutex\n");

  k_mutex_unlock(test_mux);

  cprintf("3: Mutex released\n");

  k_thread_exit();
}

/**
 * Main kernel function.
 *
 * The bootstrap processor starts running C code here.
 */
void main(void)
{
  // Begin the memory manager initialization
  page_init_low(); // Physical page allocator (lower memory)
  vm_arch_init();       // Memory management unit and kernel mappings

  // Now we can initialize the console to print messages during initialization
  

  // Complete the memory manager initialization
  page_init_high(); // Physical page allocator (higher memory)

  // Initialize core services
  k_object_pool_system_init();
  k_mutex_system_init();
  k_semaphore_system_init();
  k_mailbox_system_init();
  k_sched_init();

  vm_space_init();  // Virtual memory manager

  interrupt_init();     // Interrupt controller

  // Initialize the device drivers

  console_init(); // Console driver
  rtc_init(); // Real-time clock
  sd_init();  // MultiMedia Card
  eth_init(); // Ethernet

  // Initialize the remaining kernel services
  buf_init();     // Buffer cache
  file_init();    // File table
  
  pipe_init();    // Pipes subsystem
  process_init(); // Process table
  signal_init_system(); // Signals
  net_init();     // Network

  // test_mux = k_mutex_create("test");

  // test1 = k_thread_create(NULL, test_func1, NULL, 1);
  // test2 = k_thread_create(NULL, test_func2, NULL, 2);
  // test3 = k_thread_create(NULL, test_func3, NULL, 3);

  // k_thread_resume(test1);
  
  // k_thread_resume(test3);

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
