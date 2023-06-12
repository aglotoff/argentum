#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/drivers/console.h>
// #include <kernel/drivers/eth.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/file.h>
#include <kernel/irq.h>
#include <kernel/ksemaphore.h>
#include <kernel/ktimer.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>
#include <kernel/process.h>
#include <kernel/sd.h>

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

  // core_test();

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

struct Task th1, th2;

static uint8_t th1_stack[4096];
static uint8_t th2_stack[4096];

static void
on_task_destroy(struct Task *t)
{
  (void) t;

  cprintf("Task destroyed\n");
}

struct TaskHooks hooks = {
  .destroy = on_task_destroy,
};

struct KTimer tmr1, tmr2, tmr3;

void
timer_func(void *arg)
{
  cprintf("Timer %p\n", arg);
}

void
timer_func2(void *a)
{
  ktimer_destroy((struct KTimer *) a);
}

struct KSemaphore sem;

static void
th1_func(void)
{
  int i;

  i = ksem_get(&sem, 1000, 1);

  cprintf("Got: %d\n", i);
}

static void
th2_func(void)
{
  // int i;

  task_sleep(500);

  ksem_put(&sem);

  // for (i = 0; i < 2000; i++)
  //   task_yield();

  // cprintf("Hello\n");
  // task_sleep(1000);
  // cprintf("Bye\n");
}

void
core_test(void)
{
  ksem_create(&sem, 0);

  task_create(&th1, th1_func, 0, th1_stack + 4096, &hooks);
  task_resume(&th1);

  task_create(&th2, th2_func, 0, th2_stack + 4096, &hooks);
  task_resume(&th2);
}
