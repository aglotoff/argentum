#include <kernel/vm.h>
#include <arch/kernel/mmu.h>

#define MAKE_L1_SECTION(pa, ap) \
  ((pa) | L1_DESC_TYPE_SECT | L1_DESC_SECT_AP(ap))

// Initial translation table to "get off the ground"
__attribute__((__aligned__(L1_TABLE_SIZE))) l1_desc_t
entry_pgdir[L1_NR_ENTRIES] = {
  // Identity mapping for the first 1MB of physical memory (just enough to
  // load the entry point code):
  [0x0]                            = MAKE_L1_SECTION(0x000000, AP_PRIV_RW),

  // Higher-half mappings for the first 16MB of physical memory (should be
  // enough to initialize the page allocator, setup the master translation table
  // and allocate the LCD framebuffer):
  [L1_IDX(VIRT_KERNEL_BASE) + 0x0] = MAKE_L1_SECTION(0x000000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x1] = MAKE_L1_SECTION(0x100000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x2] = MAKE_L1_SECTION(0x200000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x3] = MAKE_L1_SECTION(0x300000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x4] = MAKE_L1_SECTION(0x400000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x5] = MAKE_L1_SECTION(0x500000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x6] = MAKE_L1_SECTION(0x600000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x7] = MAKE_L1_SECTION(0x700000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x8] = MAKE_L1_SECTION(0x800000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0x9] = MAKE_L1_SECTION(0x900000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0xA] = MAKE_L1_SECTION(0xA00000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0xB] = MAKE_L1_SECTION(0xB00000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0xC] = MAKE_L1_SECTION(0xC00000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0xD] = MAKE_L1_SECTION(0xD00000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0xE] = MAKE_L1_SECTION(0xE00000, AP_PRIV_RW),
  [L1_IDX(VIRT_KERNEL_BASE) + 0xF] = MAKE_L1_SECTION(0xF00000, AP_PRIV_RW),

  [L1_IDX(0x90000000)]       = MAKE_L1_SECTION(0x10000000, AP_PRIV_RW),
  [L1_IDX(0x9F000000)]       = MAKE_L1_SECTION(0x1F000000, AP_PRIV_RW),
};
