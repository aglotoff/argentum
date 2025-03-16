#ifndef _ARCH_MEMLAYOUT_H
#define _ARCH_MEMLAYOUT_H

/** The number of bytes mapped by a single physical page. */
#define PAGE_SIZE         4096U
/** Log2 of PAGE_SIZE. */
#define PAGE_SHIFT        12

/** Size of a kernel-mode task stack in bytes */
#define KSTACK_SIZE       PAGE_SIZE

/** Size of a user-mode process stack in bytes */
// TODO: keep in sync with rlimit
#define USTACK_SIZE       (PAGE_SIZE * 48)

/** Physical address the kernel executable is loaded at */
#define PHYS_KERNEL_LOAD  0x00100000
/** Maximum physical memory available during the early boot process */
#define PHYS_ENTRY_LIMIT  0x00400000
/** Maximum available physical memory */
#define PHYS_LIMIT        0x10000000

#define PHYS_IOAPIC_BASE  0xFEC00000
#define PHYS_LAPIC_BASE   0xFEE00000

/** All physical memory is mapped at this virtual address */
#define VIRT_KERNEL_BASE  0x80000000
/** Top of the user-mode process stack */
#define VIRT_USTACK_TOP   VIRT_KERNEL_BASE

#define VIRT_IOAPIC_BASE  0xFFFFE000
#define VIRT_LAPIC_BASE   0xFFFFF000

#define ACPI_RSDT_SIZE    0x10000
#define VIRT_ACPI_RSDT    (VIRT_IOAPIC_BASE - ACPI_RSDT_SIZE)

#define ACPI_MADT_SIZE    0x10000
#define VIRT_ACPI_MADT    (VIRT_ACPI_RSDT - ACPI_MADT_SIZE)

#endif  // !_ARCH_MEMLAYOUT_H
