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
  // First address after the kernel image
  extern uint8_t _end[];

  void *ext_kernel_end;

  boot_mem_init(_end, PA2KVA(PHYS_ENTRY_LIMIT));

  page_init(PHYS_LIMIT);  // Physical page allocator (lower memory)

	ext_kernel_end = boot_mem_alloc(0);
  boot_mem_fini();

  // Place pages mapped by 'entry_pgdir' to the free list.
  // FIXME: add low memory to region
  page_free_region(KVA2PA(ext_kernel_end), PHYS_ENTRY_LIMIT);

	arch_vm_init();

  // TODO: detect the actual amount of physical memory!
  page_free_region(PHYS_ENTRY_LIMIT, PHYS_LIMIT);

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

// Whether the bootstrap processor has finished its initialization?
int bsp_started;

void
arch_init_smp(void)
{
  // Unblock other CPUs
  bsp_started = 1;
}
