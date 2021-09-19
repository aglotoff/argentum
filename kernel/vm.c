#include <assert.h>

#include "armv7.h"
#include "console.h"
#include "mmu.h"
#include "page.h"
#include "vm.h"

tte_t *kern_trtab;

static pte_t *vm_walk_trtab(tte_t *, uintptr_t, int);
static void vm_map_region(tte_t *, uintptr_t, uint32_t, size_t, int, int);

void
vm_init(void)
{
  extern uint8_t _start[];

  struct PageInfo *p;

  // Create the master translation table
  if ((p = page_alloc(PAGE_ORDER_TRTAB, PAGE_ALLOC_ZERO)) == NULL)
    panic("out of memory");

  kern_trtab = (tte_t *) page2kva(p);

  // Map all of physical memory at KERNEL_BASE
  // Permissions: kernel RW, user NONE
  vm_map_region(kern_trtab, KERNEL_BASE, 0, PHYS_TOP, AP_PRIV_RW, 0);

  // Map exception vectors at VECTORS_BASE
  // Permissions: kernel R, user NONE
  vm_map_region(kern_trtab, VECTORS_BASE, (uint32_t) _start,
                PAGE_SMALL_SIZE, AP_PRIV_RO, 0);

  vm_init_percpu();
}

void
vm_init_percpu(void)
{
  // Switch from the minimal entry translation table to the full translation
  // table.
  write_ttbr0(PADDR(kern_trtab));
  write_ttbr1(PADDR(kern_trtab));

  // Size of the TTBR0 translation table is 8KB.
  write_ttbcr(1);

  tlbiall();
}

static pte_t *
vm_walk_trtab(tte_t *trtab, uintptr_t va, int alloc)
{
  struct PageInfo *p;
  tte_t *tte;
  pte_t *pgtab;

  tte = &trtab[TTX(va)];
  if ((*tte & TTE_TYPE_MASK) == TTE_TYPE_FAULT) {
    if (!alloc)
      return NULL;

    if ((p = page_alloc(PAGE_ORDER_PGTAB, PAGE_ALLOC_ZERO)) == NULL)
      return NULL;
    
    p->ref_count++;
    *tte = page2pa(p) | TTE_TYPE_PGTAB;
    pgtab = page2kva(p);
  } else if ((*tte & TTE_TYPE_MASK) != TTE_TYPE_PGTAB) {
    warn("tte type is not page table");
    return NULL;
  } else {
    pgtab = KADDR(TTE_PGTAB_ADDR(*tte));
  }

  return &pgtab[PTX(va)];
}

void *
vm_map_mmio(uint32_t pa, size_t n)
{
  static uint8_t *base = (uint8_t *) MMIO_BASE;
  void *ret;

  ret = base;
  base += n;

  if (base > (uint8_t *) MMIO_LIMIT)
    panic("out of memory");

  vm_map_region(kern_trtab, (uintptr_t) ret, pa, n, AP_PRIV_RW, 1);
  return ret;
}

struct PageInfo *
vm_lookup_page(tte_t *trtab, void *va, pte_t **pte_store)
{
  pte_t *pte;

  if ((pte = vm_walk_trtab(trtab, (uintptr_t ) va, 0)) == NULL)
    return NULL;
  if ((*pte & PTE_TYPE_MASK) != PTE_TYPE_SMALL)
    return NULL;

  if (pte_store)
    *pte_store = pte;
  
  return pa2page(PTE_SMALL_ADDR(*pte));
}

int
vm_insert_page(tte_t *trtab, struct PageInfo *p, void *va, int perm)
{
  pte_t *pte;

  if ((pte = vm_walk_trtab(trtab, (uintptr_t ) va, 1)) == NULL)
    return -1;

  p->ref_count++;
  vm_remove_page(trtab, va);
  *pte = page2pa(p) | PTE_AP(perm) | PTE_TYPE_SMALL;

  return 0;
}

void
vm_remove_page(tte_t *trtab, void *va)
{
  struct PageInfo *p;
  pte_t *pte;

  if ((p = vm_lookup_page(trtab, va, &pte)) == NULL)
    return;

  page_decref(p, 0);
  *pte = 0;
}

// 
// Map [va, va + n) of virtual address space to physical [pa, pa + n) in
// the translation page trtab.
//
// This function is only intended to set up static mappings in the kernel's
// portion of address space.
//
static void
vm_map_region(tte_t *trtab,
              uintptr_t va, uint32_t pa, size_t n,
              int perm, int dev)
{
  pte_t *pte;
  tte_t *tte;

  assert(va % PAGE_SMALL_SIZE == 0);
  assert(pa % PAGE_SMALL_SIZE == 0);
  assert(n % PAGE_SMALL_SIZE == 0);

  while (n != 0) {
    // Whenever possible, map entire 1MB sections to reduce the memory
    // management overhead.
    if ((va % PAGE_SECT_SIZE == 0) && (pa % PAGE_SECT_SIZE == 0) &&
        (n % PAGE_SECT_SIZE == 0)) {
      tte = &trtab[TTX(va)];
      if (*tte)
        panic("remap");

      *tte = pa | TTE_SECT_AP(perm) | TTE_TYPE_SECT;
      if (!dev)
        *tte |= (TTE_SECT_C | TTE_SECT_B);

      va += PAGE_SECT_SIZE;
      pa += PAGE_SECT_SIZE;
      n -= PAGE_SECT_SIZE;
    } else {
      if ((pte = vm_walk_trtab(trtab, va, 1)) == NULL)
        panic("out of memory");

      if (*pte)
        panic("remap %p", *pte);

      *pte = pa | PTE_AP(perm) | PTE_TYPE_SMALL;
      if (!dev)
        *pte |= (PTE_C | PTE_B);

      va += PAGE_SMALL_SIZE;
      pa += PAGE_SMALL_SIZE;
      n -= PAGE_SMALL_SIZE;
    }
  }
}
