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

enum {
  PCI_CONFIG_ADDRESS = 0xCF8,
  PCI_CONFIG_DATA    = 0xCFC,
};

enum {
  PCI_VENDOR_ID    = 0x00,
  PCI_COMMAND      = 0x04,
  PCI_SUBCLASS     = 0x0A,
  PCI_CLASS        = 0x0B,
  PCI_HEADER_TYPE  = 0x0E,
  PCI_BAR0         = 0x10,
  PCI_BAR1         = 0x14,
  PCI_BAR2         = 0x18,
  PCI_BAR3         = 0x1C,
  PCI_BAR4         = 0x20,
};

enum {
  PCI_HEADER_TYPE_MASK      = 0x7f,
  PCI_HEADER_TYPE_MULTIFUNC = (1 << 7),
};

enum {
  PCI_CLASS_MASS_STORAGE = 0x1,
};

enum {
  PCI_SUBCLASS_IDE = 0x1,
};

enum {
  PCI_COMMAND_IO         = (1 << 0),
  PCI_COMMAND_MEMORY     = (1 << 1),
  PCI_COMMAND_BUS_MASTER = (1 << 2),
};

static void
pci_config_set_address(unsigned bus, unsigned dev, unsigned func, unsigned off)
{
  outl(PCI_CONFIG_ADDRESS,
       (1U << 31) |
       ((bus & 0xFF) << 16) |
       ((dev & 0x1F) << 11) |
       ((func & 0x7) << 8) |
       ((off & 0xFC)));
}

uint8_t
pci_config_read8(unsigned bus, unsigned dev, unsigned func, unsigned off)
{
  pci_config_set_address(bus, dev, func, off & ~0x3);
  return inb(PCI_CONFIG_DATA + (off & 0x3));
}

uint16_t
pci_config_read16(unsigned bus, unsigned dev, unsigned func, unsigned off)
{
  pci_config_set_address(bus, dev, func, off & ~0x3);
  return inw(PCI_CONFIG_DATA + (off & 0x2));
}

uint32_t
pci_config_read32(unsigned bus, unsigned dev, unsigned func, unsigned off)
{
  pci_config_set_address(bus, dev, func, off);
  return inl(PCI_CONFIG_DATA);
}

void
pci_config_write16(unsigned bus, unsigned dev, unsigned func, unsigned off,
                   uint16_t data)
{
  pci_config_set_address(bus, dev, func, off & ~0x3);
  outw(PCI_CONFIG_DATA + (off & 0x2), data);
}

static void
pci_function_enable(unsigned bus, unsigned dev, unsigned func)
{
  pci_config_write16(bus, dev, func, PCI_COMMAND, PCI_COMMAND_IO |
                                                  PCI_COMMAND_MEMORY |
                                                  PCI_COMMAND_BUS_MASTER);
}

void
pci_function_check(unsigned bus, unsigned dev, unsigned func)
{
  uint8_t class_code, subclass;

  class_code = pci_config_read8(bus, dev, func, PCI_CLASS);
  subclass = pci_config_read8(bus, dev, func, PCI_SUBCLASS);

  switch (class_code) {
  case PCI_CLASS_MASS_STORAGE:
    switch (subclass) {
      case PCI_SUBCLASS_IDE:
        pci_function_enable(bus, dev, func);
        ide_init(pci_config_read32(bus, dev, func, PCI_BAR0),
                 pci_config_read32(bus, dev, func, PCI_BAR1),
                 pci_config_read32(bus, dev, func, PCI_BAR2),
                 pci_config_read32(bus, dev, func, PCI_BAR3),
                 pci_config_read32(bus, dev, func, PCI_BAR4));
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
pci_device_check(unsigned bus, unsigned dev)
{
  uint8_t header_type;
  
  if (pci_config_read16(bus, dev, 0, PCI_VENDOR_ID) == 0xFFFF)
    return;

  header_type = pci_config_read8(bus, dev, 0, PCI_HEADER_TYPE);
  if ((header_type & PCI_HEADER_TYPE_MASK) != 0)
    return; // TODO

  pci_function_check(bus, dev, 0);
  
  if (header_type & PCI_HEADER_TYPE_MULTIFUNC) {
    uint8_t func;

    for (func = 1; func < 8; func++) {
      if (pci_config_read16(bus, dev, func, PCI_VENDOR_ID) == 0xFFFF)
        continue;

      pci_function_check(bus, dev, func);
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
