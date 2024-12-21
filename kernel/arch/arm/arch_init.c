#include <stdint.h>

#include <arch/arm/mach.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/interrupt.h>

void main(void);
void mp_main(void);

void
arch_init(uintptr_t mach_type)
{
  // Initialize the memory manager
  page_init_low();  // Physical page allocator (lower memory)
  arch_vm_init();   // Memory management unit and kernel mappings
  page_init_high(); // Physical page allocator (higher memory)

  // Initialize the machine
  mach_init(mach_type);

  // Initialize the interrupt controller
  arch_interrupt_init();

  main();
}

void
arch_init_devices(void)
{
  mach_current->storage_init();
  mach_current->eth_init();
}

/**
 * Initialization code for non-boot (AP) processors.
 *
 * AP processors jump here from 'entry.S'.
 */
void
arch_mp_init(void)
{
  // Per-CPU initialization
  arch_vm_init_percpu(); // Load the kernel page table
  arch_interrupt_init_percpu();

  mp_main();
}

void
arch_eth_write(const void *buf, size_t n)
{
  mach_current->eth_write(buf, n);
}
