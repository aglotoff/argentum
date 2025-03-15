#include <stdint.h>

#include <kernel/console.h>

#include <arch/i386/lapic.h>
#include <arch/trap.h>

enum {
  REG_ID            = (0x020 >> 2), // Local APIC ID Register
  REG_VERSION       = (0x030 >> 2), // Local APIC ID Register
  REG_TPR           = (0x080 >> 2), // Task Priority Register
  REG_EOI           = (0x0b0 >> 2), // EOI Register
  REG_SPURIOUS      = (0x0f0 >> 2), // Spurious Interrupt Vector Register
  REG_LVT_TIMER     = (0x320 >> 2), // LVT Timer Register
  REG_INITIAL_COUNT = (0x380 >> 2), // Initial Count Register (for Timer)
  REG_CURRENT_COUNT = (0x390 >> 2), // Current Count Register (for Timer)
  REG_DIVIDE_CONF   = (0x3e0 >> 2), // Divide Configuration Register (for Timer)
};

enum {
  SPURIOUS_ENABLE   = (1 << 8),
};

enum {
  DIVIDE_CONF_X1    = 0xB,
};

enum {
  LVT_TIMER_PERIODIC = (1 << 17),
};

static volatile uint32_t *lapic_base = (uint32_t *) 0xFEE00000;

void
lapic_reg_write(int idx, uint32_t value)
{
  lapic_base[idx] = value;
  (void) lapic_base[REG_ID];
}

void
lapic_init(void)
{
  lapic_reg_write(REG_SPURIOUS, SPURIOUS_ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  lapic_reg_write(REG_DIVIDE_CONF, DIVIDE_CONF_X1);
  lapic_reg_write(REG_LVT_TIMER, LVT_TIMER_PERIODIC | (T_IRQ0 + IRQ_PIT));
  lapic_reg_write(REG_INITIAL_COUNT, 10000000); // TODO: calibrate

  lapic_reg_write(REG_TPR, 0);
}

void
lapic_eoi(void)
{
  lapic_reg_write(REG_EOI, 0);
}
