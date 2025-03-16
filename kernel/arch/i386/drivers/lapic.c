#include <stdint.h>

#include <kernel/console.h>

#include <arch/memlayout.h>
#include <arch/i386/lapic.h>
#include <arch/i386/i8253.h>
#include <arch/i386/io.h>
#include <arch/trap.h>
#include <kernel/vm.h>

enum {
  REG_ID            = (0x020 >> 2), // Local APIC ID Register
  REG_VERSION       = (0x030 >> 2), // Local APIC ID Register
  REG_TPR           = (0x080 >> 2), // Task Priority Register
  REG_EOI           = (0x0b0 >> 2), // EOI Register
  REG_SPURIOUS      = (0x0f0 >> 2), // Spurious Interrupt Vector Register
  REG_ESR           = (0x280 >> 2),
  REG_ICR_LO        = (0x300 >> 2),
  REG_ICR_HI        = (0x310 >> 2),
  REG_LVT_TIMER     = (0x320 >> 2), // LVT Timer Register
  REG_LVT_PEFORM    = (0x340 >> 2),
  REG_LVT_LINT0     = (0x350 >> 2),
  REG_LVT_LINT1     = (0x360 >> 2),
  REG_LVT_ERROR     = (0x370 >> 2),
  REG_INITIAL_COUNT = (0x380 >> 2), // Initial Count Register (for Timer)
  REG_CURRENT_COUNT = (0x390 >> 2), // Current Count Register (for Timer)
  REG_DIVIDE_CONF   = (0x3e0 >> 2), // Divide Configuration Register (for Timer)
};

enum {
  SPURIOUS_ENABLE   = (1 << 8),
};

enum {
  ICR_BCAST       = 0x00080000,
  ICR_STARTUP     = 0x00000600,
  ICR_ASSERT      = 0x00004000,
  ICR_INIT        = 0x00000500,
  ICR_DELIV_STS   = 0x00001000,
  ICR_LEVEL       = 0x00008000
};

enum {
  DIVIDE_CONF_X1    = 0xB,
};

enum {
  LVT_MASKED         = (1 << 16),
  LVT_TIMER_PERIODIC = (1 << 17),
};

uint32_t lapic_pa;
size_t lapic_ncpus;

static volatile uint32_t *lapic_base = (uint32_t *) VIRT_LAPIC_BASE;

void
lapic_reg_write(int idx, uint32_t value)
{
  lapic_base[idx] = value;
  (void) lapic_base[REG_ID];
}

void
lapic_init_percpu(void)
{
  uint32_t init_count, count;

  lapic_reg_write(REG_SPURIOUS, SPURIOUS_ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // Calibrate
  lapic_reg_write(REG_DIVIDE_CONF, DIVIDE_CONF_X1);
  lapic_reg_write(REG_INITIAL_COUNT, 0xFFFFFFFF);
  i8253_count_down();
  init_count = lapic_base[REG_CURRENT_COUNT];

  count = 0xffffffff - init_count;

  lapic_reg_write(REG_DIVIDE_CONF, DIVIDE_CONF_X1);
  lapic_reg_write(REG_LVT_TIMER, LVT_TIMER_PERIODIC | (T_IRQ0 + IRQ_PIT));
  lapic_reg_write(REG_INITIAL_COUNT, count);

  lapic_reg_write(REG_LVT_LINT0, LVT_MASKED);
  lapic_reg_write(REG_LVT_LINT1, LVT_MASKED);

  if (((lapic_base[REG_VERSION] >> 16) & 0xFF) >= 4)
    lapic_reg_write(REG_LVT_PEFORM, LVT_MASKED);

  lapic_reg_write(REG_LVT_ERROR, T_IRQ0 + IRQ_ERROR);

  lapic_reg_write(REG_ESR, 0);
  lapic_reg_write(REG_ESR, 0);

  lapic_eoi();

  lapic_reg_write(REG_ICR_HI, 0);
  lapic_reg_write(REG_ICR_LO, ICR_BCAST | ICR_INIT | ICR_LEVEL);
  while (lapic_base[REG_ICR_LO] & ICR_DELIV_STS)
    ;

  lapic_reg_write(REG_TPR, 0);
}

void
lapic_init(void)
{
  arch_vm_map_fixed(VIRT_LAPIC_BASE, lapic_pa, PAGE_SIZE,
                    PROT_READ | PROT_WRITE);

  lapic_init_percpu();
}

void
lapic_eoi(void)
{
  lapic_reg_write(REG_EOI, 0);
}

unsigned
lapic_id(void)
{
  // TODO: this is workaround for early boot
  return lapic_pa ? lapic_base[REG_ID] >> 24 : 0;
}

#define CMOS_PORT    0x70

void
microdelay()
{

}

void
lapic_start(unsigned cpu_id, uintptr_t addr)
{
  int i;
  volatile uint16_t *wrv;

  // "The BSP must initialize CMOS shutdown code to 0AH
  // and the warm reset vector (DWORD based at 40:67) to point at
  // the AP startup code prior to the [universal startup algorithm]."
  outb(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
  outb(CMOS_PORT+1, 0x0A);

  wrv = (uint16_t*) PA2KVA(((0x40 << 4) | 0x67));  // Warm reset vector
  wrv[0] = 0;
  wrv[1] = addr >> 4;

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  lapic_reg_write(REG_ICR_HI, cpu_id << 24);
  lapic_reg_write(REG_ICR_LO, ICR_INIT | ICR_LEVEL | ICR_ASSERT);
  while (lapic_base[REG_ICR_LO] & ICR_DELIV_STS)
    ;
  microdelay();
  lapic_reg_write(REG_ICR_LO, ICR_INIT | ICR_LEVEL);
  while (lapic_base[REG_ICR_LO] & ICR_DELIV_STS)
    ;
  microdelay();

  // Send startup IPI (twice!) to enter code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for(i = 0; i < 2; i++){
    lapic_reg_write(REG_ICR_HI, cpu_id << 24);
    lapic_reg_write(REG_ICR_LO, ICR_STARTUP | (addr >> 12));
    while (lapic_base[REG_ICR_LO] & ICR_DELIV_STS)
    ;
    microdelay();
  }
}
