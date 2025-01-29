#include <string.h>

#include <kernel/core/irq.h>
#include <kernel/core/tick.h>

#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/interrupt.h>
#include <kernel/console.h>
#include <kernel/tty.h>
#include <kernel/trap.h>

#include <arch/i386/ide.h>
#include <arch/i386/io.h>

void main(void);
void mp_main(void);

void
arch_init(void)
{
	page_init_low();
	arch_vm_init();
	page_init_high();

  arch_interrupt_init();

  main();
}

uint32_t
pci_config_read(uint32_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t address;
  uint32_t lbus  = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  // Create configuration address as per Figure 1
  address = (uint32_t)((lbus << 16) | (lslot << 11) |
            (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

  // Write out the address
  outl(0xCF8, address);

  // Read in the data
  // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
  return inl(0xCFC);
}

void
pci_config_write(uint32_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data) {
  uint32_t address;
  uint32_t lbus  = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  // Create configuration address as per Figure 1
  address = (uint32_t)((lbus << 16) | (lslot << 11) |
            (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

  // Write out the address
  outl(0xCF8, address);

  // Read in the data
  // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
  outl(0xCFC, data);
}

void
pci_function_check(uint8_t bus, uint8_t device, uint8_t function)
{
  uint32_t data;
  uint8_t class_code, subclass;

  data = pci_config_read(bus, device, function, 0x8);
  class_code = (data >> 24) & 0xFF;
  subclass = (data >> 16) & 0xFF;

  switch (class_code) {
  case 0x1:
    switch (subclass) {
      case 0x1:
        data = pci_config_read(bus, device, function, 0x4);
        pci_config_write(bus, device, function, 0x4, data | 0x4);

        ide_init(pci_config_read(bus, device, function, 0x10),
                 pci_config_read(bus, device, function, 0x14),
                 pci_config_read(bus, device, function, 0x18),
                 pci_config_read(bus, device, function, 0x1c),
                 pci_config_read(bus, device, function, 0x20));
        break;
      default:
        break;
    }
    break;
  default:
    break;
  }
}

void
pci_device_check(uint8_t bus, uint8_t device) {
  uint16_t vendor_id;
  uint8_t header_type;
  
  if ((vendor_id = pci_config_read(bus, device, 0, 0)) == 0xFFFF)
    return;

  pci_function_check(bus, device, 0);

  header_type = (pci_config_read(bus, device, 0, 0xC) >> 16) & 0xFF;
  if (header_type & 0x80) {
    uint8_t f;

    for (f = 1; f < 8; f++) {
      if ((vendor_id = pci_config_read(bus, device, 0, 0)) == 0xFFFF)
        continue;

      pci_function_check(bus, device, f);
    }
  }
}

void
pci_scan(void)
{
  uint16_t bus;
  uint8_t device;

  for (bus = 0; bus < 256; bus++) {
    for (device = 0; device < 32; device++) {
      pci_device_check(bus, device);
    }
  }
}

void
arch_init_devices(void)
{
  interrupt_attach(0, timer_irq, NULL);
  pci_scan();
}

void
arch_mp_init(void)
{
  // TODO
}

void
arch_eth_write(const void *buf, size_t n)
{
  // TODO
  (void) buf;
  (void) n;
}
