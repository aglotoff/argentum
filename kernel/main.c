#include <kernel/assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include <kernel/console.h>
#include <kernel/core/cpu.h>
#include <kernel/core/condvar.h>
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
#include <kernel/interrupt.h>
#include <kernel/time.h>

// Whether the bootstrap processor has finished its initialization?
int bsp_started;

// For uname()
struct utsname utsname = {
  .sysname = "Argentum",
  .nodename = "localhost",
  .release = "0.1.0",
  .version = __DATE__ " " __TIME__,
#if defined __arm__
  .machine = "arm",
#elif defined __i386__
  .machine = "i386",
#endif
};

void arch_init_devices(void);

void
mp_main(void)
{
  cprintf("Starting CPU %d\n", k_cpu_id());

  // Enter the scheduler loop
  k_sched_start();
}

void
test_task(void *arg)
{
  for (;;) {
    cprintf("%s", arg);
  }
}

/**
 * Main kernel function.
 *
 * The bootstrap processor starts running C code here.
 */
void
main(void)
{
  // Initialize core services
  k_object_pool_system_init();
  k_condvar_system_init();
  k_mutex_system_init();
  k_semaphore_system_init();
  k_mailbox_system_init();
  k_sched_init();

  // Initialize device drivers
  tty_init();                   // Console
  arch_init_devices();

  // Initialize the remaining kernel services
  buf_init();           // Buffer cache
  file_init();          // File table
  vm_space_init();      // Virtual memory manager
  pipe_init();          // Pipes
  process_init();       // Process table
  //net_init();           // Networking

  // ipc_init();

  time_init();

  // Unblock other CPUs
  bsp_started = 1;

  // struct KTask *t1, *t2;

  // t1 = k_task_create(NULL, test_task, "AAAA", 1);
  // t2 = k_task_create(NULL, test_task, "BBBB", 1);
 // k_task_resume(t1);
 // k_task_resume(t2);

  mp_main();
}

