#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/drivers/console.h>
// #include <argentum/drivers/eth.h>
#include <argentum/drivers/rtc.h>
#include <argentum/fs/buf.h>
#include <argentum/fs/file.h>
#include <argentum/irq.h>
#include <argentum/mm/kmem.h>
#include <argentum/mm/mmu.h>
#include <argentum/mm/page.h>
#include <argentum/mm/vm.h>
#include <argentum/process.h>
#include <argentum/sd.h>

static void mp_main(void);
void core_test(void);

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
  irq_init();           // Interrupt controller
  console_init();       // Console driver

  // Complete the memory manager initialization
  page_init_high();     // Physical page allocator (higher memory)
  kmem_init();          // Object allocator
  vm_init();            // Virtual memory manager

  // Initialize the device drivers
  rtc_init();           // Real-time clock
  sd_init();            // MultiMedia Card
  // eth_init();        // Ethernet

  // Initialize the remaining kernel services
  buf_init();           // Buffer cache
  file_init();          // File table
  sched_init();         // Scheduler
  process_init();       // Process table

  //core_test();

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

uint8_t th1_stack[4096];
uint8_t th2_stack[4096];

static void
th1_func(void)
{
  //cpu_irq_enable();

  for (;;)
    cprintf("AAAAA");
}

static void
th2_func(void)
{
  //cpu_irq_enable();

  for (;;)
    cprintf("BBBBB");
}

void
core_test(void)
{
  struct KThread *th1, *th2;

  th1 = kthread_create(NULL, th1_func, 0, th1_stack + 4096);
  kthread_resume(th1);

  th2 = kthread_create(NULL, th2_func, 0, th2_stack + 4096);
  kthread_resume(th2);
}
