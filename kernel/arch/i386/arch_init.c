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
#include <arch/i386/lapic.h>
#include <arch/i386/ioapic.h>

void acpi_init(void);
void main(void);
void mp_main(void);

void
arch_init(void)
{
	page_init_low();
	arch_vm_init();
  page_init_high();

#ifndef NOSMP
  acpi_init();
#endif

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

struct Rsdp {
  char      signature[8];
  uint8_t   checksum;
  char      oemid[6];
  uint8_t   revision;
  uint32_t  rsdt_address;
} __attribute__ ((packed));

struct AcpiSdtHeader {
  char     signature[4];
  uint32_t length;
  uint8_t  revision;
  uint8_t  checksum;
  char     oemid[6];
  char     oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
};

struct AcpiRsdt {
  struct AcpiSdtHeader header;
  uint32_t             pointers[0];
};

struct AcpiMadt {
  struct AcpiSdtHeader header;
  uint32_t             lapic_addr;
  uint32_t             flags;
};

struct AcpiMadtEntry {
  uint8_t type;
  uint8_t length;
} __attribute__((packed));

enum {
  ACPI_MADT_LAPIC = 0,
  ACPI_MADT_IOAPIC = 1,
};

struct AcpiMadtEntryLapic {
  uint8_t type;
  uint8_t length;
  uint8_t acpi_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__((packed));

struct AcpiMadtEntryIOApic {
  uint8_t  type;
  uint8_t  length;
  uint8_t  io_apic_id;
  uint8_t  reserved;
  uint32_t io_apic_address;
  uint32_t intr_base;
} __attribute__((packed));

static uint8_t
acpi_sum(void *addr, size_t n)
{
  uint8_t *p = (uint8_t *) addr;
  uint8_t sum = 0;
  size_t i;
 
  for (i = 0; i < n; i++)
    sum += p[i];

  return sum;
}

static struct Rsdp *
acpi_check_rsdp(void *addr, size_t n)
{
  uint8_t *p = (uint8_t *) addr;
  uint8_t *e = p + n;
  
  for ( ; p < e; p += 16) {
    if (memcmp(p, "RSD PTR ", 8) != 0)
      continue;
    if (acpi_sum(p, sizeof(struct Rsdp)) != 0)
      continue;
    return (struct Rsdp *) p;
  }

  return NULL;
}

static struct Rsdp *
acpi_find_rsdp(void)
{
  struct Rsdp *rsdp;
  uint8_t *bda = (uint8_t *) PA2KVA(0x40E);
  uint16_t ebda = (bda[1] << 8) | (bda[0] << 4);
  
  if ((rsdp = acpi_check_rsdp(PA2KVA(ebda), 0x1000)) != NULL)
    return rsdp;

  return acpi_check_rsdp(PA2KVA(0x000E0000), 0x20000);
}

static struct AcpiRsdt *
acpi_rsdt_map(uint32_t rsdt_address)
{
  struct AcpiSdtHeader *rsdt;
  uintptr_t rsdt_va;

  arch_vm_map_fixed(VIRT_ACPI_RSDT,
                    ROUND_DOWN(rsdt_address, PAGE_SIZE),
                    ACPI_RSDT_SIZE,
                    PROT_READ | PROT_WRITE);

  rsdt_va = VIRT_ACPI_RSDT + (rsdt_address % PAGE_SIZE);
  rsdt = (struct AcpiSdtHeader *) rsdt_va;

  if (rsdt_va + rsdt->length > VIRT_ACPI_RSDT + ACPI_RSDT_SIZE)
    k_panic("too large RSDT");
  if (memcmp(rsdt->signature, "RSDT", 4) != 0)
    k_panic("bad RSDT");
  if (acpi_sum(rsdt, rsdt->length) != 0)
    k_panic("bad RSDT");

  return (struct AcpiRsdt *) rsdt;
}

static void
acpi_rsdt_unmap(void)
{
  arch_vm_unmap_fixed(VIRT_ACPI_RSDT, ACPI_RSDT_SIZE);
}

static void
acpi_madt_unmap(void)
{
  arch_vm_unmap_fixed(VIRT_ACPI_MADT, ACPI_MADT_SIZE);
}

static struct AcpiMadt *
acpi_madt_map(uint32_t address)
{
  struct AcpiSdtHeader *madt;
  uintptr_t va;

  arch_vm_map_fixed(VIRT_ACPI_MADT,
                    ROUND_DOWN(address, PAGE_SIZE),
                    ACPI_MADT_SIZE,
                    PROT_READ | PROT_WRITE);

  va = VIRT_ACPI_MADT + (address % PAGE_SIZE);
  madt = (struct AcpiSdtHeader *) va;

  if (va + madt->length > VIRT_ACPI_MADT + ACPI_MADT_SIZE)
    k_panic("too large SDT");

  if (memcmp(madt->signature, "APIC", 4) != 0) {
    acpi_madt_unmap();
    return NULL;
  }
  if (acpi_sum(madt, madt->length) != 0) {
    acpi_madt_unmap();
    return NULL;
  }

  return (struct AcpiMadt *) madt;
}

static uint32_t testids[3];
static int32_t testflags[3];

static void
acpi_madt_parse(struct AcpiMadt *madt)
{
  uint8_t *p = (uint8_t *) (madt + 1);
  uint8_t *e = (uint8_t *) madt + madt->header.length;

  lapic_pa = madt->lapic_addr;

  while (p < e) {
    struct AcpiMadtEntry *entry = (struct AcpiMadtEntry *) p;

    switch (entry->type) {
      case ACPI_MADT_LAPIC: {
        if (lapic_ncpus >= K_CPU_MAX)
          break;

        testids[lapic_ncpus] = ((struct AcpiMadtEntryLapic *) entry)->apic_id;
        testflags[lapic_ncpus] = ((struct AcpiMadtEntryLapic *) entry)->flags;
        // FIXME: could be not sequential
        lapic_ncpus++;
        break;
      }
      case ACPI_MADT_IOAPIC: {
        // FIXME: what if multiple?
        ioapic_pa = ((struct AcpiMadtEntryIOApic *) entry)->io_apic_address;
        break;
      }
    }

    p += entry->length;
  }
}

void
acpi_init(void)
{
  struct Rsdp *rsdp;
  struct AcpiRsdt *rsdt;
  size_t i, n;

  if ((rsdp = acpi_find_rsdp()) == NULL)
    k_panic("no RSDP");

  rsdt = acpi_rsdt_map(rsdp->rsdt_address);
  n = (rsdt->header.length - sizeof(rsdt->header)) / sizeof(rsdt->pointers[0]);

  for (i = 0; i < n; i++) {
    struct AcpiMadt *madt = acpi_madt_map(rsdt->pointers[i]);

    if (madt != NULL) {
      acpi_madt_parse(madt);
      acpi_madt_unmap();
    }
  }

  acpi_rsdt_unmap();
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

struct KSemaphore smp_sema;

void
arch_mp_entry(void)
{
  arch_vm_init_percpu();
  arch_interrupt_init_percpu();

  k_semaphore_put(&smp_sema);
  
  mp_main();
}

extern void *kernel_pgdir;

uint32_t mp_stack;

void
arch_init_smp(void)
{
  extern uint8_t mp_start[], mp_end[];

  extern uint8_t kstack_top[];

  uint32_t *mp_entry = (uint32_t *) PA2KVA(PHYS_MP_ENTRY);
  unsigned   cpu_id;

  // TODO: check size

  memmove(mp_entry, mp_start, (size_t) (mp_end - mp_start));

  k_semaphore_create(&smp_sema, 0);

  for (cpu_id = 0; cpu_id < lapic_ncpus; cpu_id++) {
    if (cpu_id == lapic_id())
      continue;
    
    mp_stack = (uint32_t) kstack_top - (cpu_id * KSTACK_SIZE);

    lapic_start(cpu_id, PHYS_MP_ENTRY);

    while (k_semaphore_try_get(&smp_sema) != 0)
      ;
  }
}
