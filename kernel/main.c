#include <stdio.h>

#include <arch/kernel/console.h>
#include <arch/kernel/trap.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/object.h>
#include <kernel/page.h>
#include <kernel/smp.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

static void
test_thread(void *arg)
{
  for (int i = 0; i < 10000; i++) {
    kprintf("%d", arg);
  }
}

void
main(void)
{
  // Initialize architecture-specific trap handling mechanism
  arch_trap_init();
  
  // Initialize the physical page allocator
  page_init();
  // Initialize architecture-specific virtual memory system
  arch_vm_init();

  // Initialize the console driver
  arch_console_init();
  // Now we can output messages
  kprintf("Argentum booting\n");
  
  // Initialize the object allocator
  object_init();

  // Initialize IRQ structures
  irq_init();
  // Initialize architecture-specific IRQ handling mechanism
  arch_irq_init();

  // Initialize threads
  thread_init();

  // Create test threads
  thread_create(test_thread, (void *) 1, 0);
  thread_create(test_thread, (void *) 2, 0);
  thread_create(test_thread, (void *) 3, 10);
  thread_create(test_thread, (void *) 4, 10);

  // Start other cores
  arch_smp_init();

  // Start the scheduler
  thread_start();
}

/**
 * Indicates that the kernel has called panic() and contains the error message
 */
const char *panic_str = NULL;

#define PANIC_MAX 1024

void
_Panic(const char *file, int line, const char *format, ...)
{
  // The message is saved inside a buffer in case the console is broken
  static char buf[PANIC_MAX];

  if (panic_str == NULL) {
    va_list ap;
    size_t n;

    panic_str = buf;

    va_start(ap, format);

    n  = snprintf(buf, PANIC_MAX, "Kernel panic on CPU %d at %s:%d: ",
                  smp_id(), file, line);
    n += vsnprintf(buf + n, PANIC_MAX - n, format, ap);
    buf[n] = '\0';

    va_end(ap);

    kprintf("%s\n", panic_str);
  }

  for (;;) {
    // Never returns
  }
}
