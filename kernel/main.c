#include <stdio.h>

#include <arch_console.h>
#include <arch_trap.h>
#include <irq.h>
#include <kernel.h>
#include <smp.h>

void
main(void)
{
  // Initialize the console driver
  arch_console_init();
  // Now we can output messages
  kprintf("Argentum booting\n");

  // Initialize architecture-specific trap handling
  arch_trap_init();
  
  // TODO: initialize the page allocator
  // TODO: initialize architecture-specific virtual memory system
  // TODO: initialize the slab allocator

  // Initialize IRQ structures
  irq_init();
  // Initialize architecture-specific IRQ handling
  arch_irq_init();

  // TODO: initialize the process manager

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
