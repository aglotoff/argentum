#include <stdint.h>

#include <kernel/console.h>

#include <arch/memlayout.h>
#include <arch/i386/ioapic.h>
#include <arch/trap.h>
#include <kernel/vm.h>

// Memory Mapped Registers for Accessing IOAPIC Registers
enum {
  IOREGSEL = (0x00 >> 2), // I/O Register Select Register
  IOWIN    = (0x10 >> 2), // I/O Window Register
};

// IOAPIC Registers
enum {
  IOAPICVER = 0x01, // IOAPIC Version Register
  IOREDTBL  = 0x10, // I/O Redirection Table Registers
};

enum {
  REDTBL_MASKED = (1 << 16),
};

uint32_t ioapic_pa;

volatile uint32_t *ioapic_base = (uint32_t *) VIRT_IOAPIC_BASE;

static uint32_t
ioapic_reg_read(int reg)
{
  ioapic_base[IOREGSEL] = reg;
  return ioapic_base[IOWIN];
}

static void
ioapic_reg_write(int reg, uint32_t data)
{
  ioapic_base[IOREGSEL] = reg;
  ioapic_base[IOWIN] = data;
}

void
ioapic_init(void)
{
  int irq, max_irq;

  arch_vm_map_fixed(VIRT_IOAPIC_BASE, ioapic_pa, PAGE_SIZE,
                    PROT_READ | PROT_WRITE);

  max_irq = (ioapic_reg_read(IOAPICVER) >> 16) & 0xFF;

  for (irq = 0; irq <= max_irq; irq++){
    ioapic_reg_write(IOREDTBL + (2 * irq), REDTBL_MASKED | (T_IRQ0 + irq));
    ioapic_reg_write(IOREDTBL + (2 * irq) + 1, 0);
  }
}

void
ioapic_enable(int irq, int cpu)
{
  ioapic_reg_write(IOREDTBL + (2 * irq), T_IRQ0 + irq);
  ioapic_reg_write(IOREDTBL + (2 * irq) + 1, cpu << 24);
}

void
ioapic_mask(int irq)
{
  uint32_t data = ioapic_reg_read(IOREDTBL + (2 * irq));
  ioapic_reg_write(IOREDTBL + (2 * irq), data | REDTBL_MASKED);
}

void
ioapic_unmask(int irq)
{
  uint32_t data = ioapic_reg_read(IOREDTBL + (2 * irq));
  ioapic_reg_write(IOREDTBL + (2 * irq), data & ~REDTBL_MASKED);
}
