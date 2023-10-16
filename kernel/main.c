#include <stdio.h>

#include <arch_console.h>
#include <arch_trap.h>
#include <irq.h>
#include <kernel.h>
#include <page.h>
#include <smp.h>
#include <vm.h>

void
main(void)
{
  // Initialize architecture-specific trap handling
  arch_trap_init();
  
  // Initialize the physical page allocator
  page_init();
  // Initialize architecture-specific virtual memory system
  arch_vm_init();

  // Initialize the console driver
  arch_console_init();
  // Now we can output messages
  kprintf("Argentum booting\n");
  
  // TODO: initialize the slab allocator

  // Initialize IRQ structures
  irq_init();
  // Initialize architecture-specific IRQ handling
  arch_irq_init();

  // TODO: initialize the process manager

  // Start other cores
  arch_smp_init();

  // TODO: start the scheduler
  irq_enable();

  for (;;) {
    asm volatile("wfi");
  }
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
