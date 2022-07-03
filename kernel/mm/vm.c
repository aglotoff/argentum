#include <assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/armv7.h>
#include <kernel/drivers/console.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/kobject.h>
#include <kernel/mm/page.h>

#include <kernel/mm/vm.h>

static pte_t *vm_walk_trtab(tte_t *, uintptr_t, int);
static void   vm_static_map(tte_t *, uintptr_t, uint32_t, size_t, int);

static struct KObjectPool *vm_pool;
// static struct KObjectPool *vm_area_pool;

/*
 * ----------------------------------------------------------------------------
 * Translation Table Initializaion
 * ----------------------------------------------------------------------------
 */

// Master kernel translation table.
static tte_t *kern_trtab;

void
vm_init(void)
{
  extern uint8_t _start[];

  struct Page *page;

  // Allocate the master translation table
  if ((page = page_alloc_block(2, PAGE_ALLOC_ZERO)) == NULL)
    panic("out of memory");

  kern_trtab = (tte_t *) page2kva(page);

  // Map all physical memory at KERNEL_BASE
  // Permissions: kernel RW, user NONE
  vm_static_map(kern_trtab, KERNEL_BASE, 0, PHYS_TOP, VM_READ | VM_WRITE);

  // Map all devices 
  vm_static_map(kern_trtab, KERNEL_BASE + PHYS_TOP, PHYS_TOP,
              VECTORS_BASE - KERNEL_BASE - PHYS_TOP,
              VM_READ | VM_WRITE | VM_NOCACHE);

  // Map exception vectors at VECTORS_BASE
  // Permissions: kernel R, user NONE
  vm_static_map(kern_trtab, VECTORS_BASE, (uint32_t) _start, PAGE_SIZE, VM_READ);

  vm_init_percpu();

  vm_pool = kobject_pool_create("vm_pool", sizeof(struct VM), 0);
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

/*
 * ----------------------------------------------------------------------------
 * Low-Level Operations
 * ----------------------------------------------------------------------------
 */

// VM proptection flags to ARM MMU AP bits
static int prot_to_ap[] = {
  [VM_READ]                      = AP_PRIV_RO, 
  [VM_WRITE]                     = AP_PRIV_RW, 
  [VM_READ | VM_WRITE]           = AP_PRIV_RW, 
  [VM_USER | VM_READ]            = AP_USER_RO, 
  [VM_USER | VM_WRITE]           = AP_BOTH_RW, 
  [VM_USER | VM_READ | VM_WRITE] = AP_BOTH_RW, 
};

static inline void
vm_pte_set_flags(pte_t *pte, int flags)
{
  *(pte + (NPTENTRIES * 2)) = flags;
}

static inline void
vm_pte_set(pte_t *pte, physaddr_t pa, int prot)
{
  int flags;

  flags = PTE_AP(prot_to_ap[prot & 7]);
  if ((prot & VM_USER) && !(prot & VM_EXEC))
    flags |= PTE_SMALL_XN;
  if (!(prot & VM_NOCACHE))
    flags |= (PTE_B | PTE_C);

  *pte = pa | flags | PTE_TYPE_SMALL;
  vm_pte_set_flags(pte, prot);
}

static inline void
vm_pte_clear(pte_t *pte)
{
  *pte = 0;
  vm_pte_set_flags(pte, 0);
}

static inline void
vm_tte_set_pgtab(tte_t *tte, physaddr_t pa)
{
  *tte = pa | TTE_TYPE_PGTAB;
}

static inline void
vm_tte_set_sect(tte_t *tte, physaddr_t pa, int prot)
{
  int flags;

  flags = TTE_SECT_AP(prot_to_ap[prot & 7]);
  if ((prot & VM_USER) && !(prot & VM_EXEC))
    flags |= TTE_SECT_XN;
  if (!(prot & VM_NOCACHE))
    flags |= (TTE_SECT_B | TTE_SECT_C);

  *tte = pa | flags | TTE_TYPE_SECT;
}

//
// Returns the pointer to the page table entry in the translation table trtab
// that corresponds to the virtual address va. If alloc is not zero, allocate
// any required page tables.
//
static pte_t *
vm_walk_trtab(tte_t *trtab, uintptr_t va, int alloc)
{
  tte_t *tte;
  pte_t *pgtab;

  tte = &trtab[TTX(va)];
  if ((*tte & TTE_TYPE_MASK) == TTE_TYPE_FAULT) {
    struct Page *page;

    if (!alloc || (page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL)
      return NULL;
    
    page->ref_count++;

    // We could fit four page tables into one physical page. But because we
    // also need to associate some metadata with each entry, we store only two
    // page tables in the bottom of the allocated physical page.
    vm_tte_set_pgtab(&trtab[(TTX(va) & ~1) + 0], page2pa(page));
    vm_tte_set_pgtab(&trtab[(TTX(va) & ~1) + 1], page2pa(page) + PGTAB_SIZE);
  } else if ((*tte & TTE_TYPE_MASK) != TTE_TYPE_PGTAB) {
    panic("not a page table");
  }

  pgtab = KADDR(TTE_PGTAB_ADDR(*tte));
  return &pgtab[PTX(va)];
}

// 
// Map [va, va + n) of virtual address space to physical [pa, pa + n) in
// the translation page trtab.
//
// This function is only intended to set up static mappings in the kernel's
// portion of address space.
//
static void
vm_static_map(tte_t *trtab, uintptr_t va, uint32_t pa, size_t n, int prot)
{ 
  assert(va % PAGE_SIZE == 0);
  assert(pa % PAGE_SIZE == 0);
  assert(n  % PAGE_SIZE == 0);

  while (n != 0) {
    // Whenever possible, map entire 1MB sections to reduce the memory
    // management overhead.
    if ((va % PAGE_SECT_SIZE == 0) &&
        (pa % PAGE_SECT_SIZE == 0) &&
        (n  % PAGE_SECT_SIZE == 0)) {
      tte_t *tte;

      tte = &trtab[TTX(va)];
      if (*tte)
        panic("remap");

      vm_tte_set_sect(tte, pa, prot);

      va += PAGE_SECT_SIZE;
      pa += PAGE_SECT_SIZE;
      n  -= PAGE_SECT_SIZE;
    } else {
      pte_t *pte;

      if ((pte = vm_walk_trtab(trtab, va, 1)) == NULL)
        panic("out of memory");
      if (*pte)
        panic("remap %p", *pte);

      vm_pte_set(pte, pa, prot);

      va += PAGE_SIZE;
      pa += PAGE_SIZE;
      n  -= PAGE_SIZE;
    }
  }
}

/*
 * ----------------------------------------------------------------------------
 * Translation Table Switch
 * ----------------------------------------------------------------------------
 */

/**
 * Load the master kernel translation table.
 */
void
vm_switch_kernel(void)
{
  write_ttbr0(PADDR(kern_trtab));
  tlbiall();
}

/**
 * Load the user process translation table.
 * 
 * @param trtab Pointer to the translation table to be loaded.
 */
void
vm_switch_user(struct VM *vm)
{
  write_ttbr0(PADDR(vm->trtab));
  tlbiall();
}

/*
 * ----------------------------------------------------------------------------
 * Page Frame Mapping Operations
 * ----------------------------------------------------------------------------
 */

struct Page *
vm_lookup_page(tte_t *trtab, const void *va, pte_t **pte_store)
{
  pte_t *pte;

  if ((uintptr_t) va >= KERNEL_BASE)
    panic("bad va: %p", va);

  pte = vm_walk_trtab(trtab, (uintptr_t ) va, 0);

  if (pte_store)
    *pte_store = pte;

  if ((pte == NULL) || ((*pte & PTE_TYPE_SMALL) != PTE_TYPE_SMALL))
    return NULL;

  return pa2page(PTE_SMALL_ADDR(*pte));
}

int
vm_insert_page(tte_t *trtab, struct Page *page, void *va, unsigned perm)
{
  pte_t *pte;

  if ((uintptr_t) va >= KERNEL_BASE)
    panic("bad va: %p", va);

  if ((pte = vm_walk_trtab(trtab, (uintptr_t ) va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference count before calling vm_remove_page() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions.
  page->ref_count++;

  // If present, remove the previous mapping.
  vm_remove_page(trtab, va);

  vm_pte_set(pte, page2pa(page), perm);

  return 0;
}

void
vm_remove_page(tte_t *trtab, void *va)
{
  struct Page *page;
  pte_t *pte;

  if ((uintptr_t) va >= KERNEL_BASE)
    panic("bad va: %p", va);

  if ((page = vm_lookup_page(trtab, va, &pte)) == NULL)
    return;

  if (--page->ref_count == 0)
    page_free_one(page);

  vm_pte_clear(pte);

  tlbimva((uintptr_t) va);
}

/*
 * ----------------------------------------------------------------------------
 * Managing Regions of Memory
 * ----------------------------------------------------------------------------
 */

int
vm_user_alloc(struct VM *vm, void *va, size_t n, int prot)
{
  struct Page *page;
  uint8_t *a, *start, *end;
  int r;

  start = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end   = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((start > end) || (end > (uint8_t *) KERNEL_BASE))
    panic("invalid range [%p,%p)", start, end);

  for (a = start; a < end; a += PAGE_SIZE) {
    if ((page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
      vm_user_dealloc(vm, start, a - start);
      return -ENOMEM;
    }

    if ((r = (vm_insert_page(vm->trtab, page, a, prot)) != 0)) {
      page_free_one(page);
      vm_user_dealloc(vm, start, a - start);
      return r;
    }
  }

  return 0;
}

void
vm_user_dealloc(struct VM *vm, void *va, size_t n)
{
  uint8_t *a, *end;
  struct Page *page;
  pte_t *pte;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((a > end) || (end > (uint8_t *) KERNEL_BASE))
    panic("invalid range [%p,%p)", a, end);

  while (a < end) {
    page = vm_lookup_page(vm->trtab, a, &pte);

    if (pte == NULL) {
      // Skip the rest of the page table 
      a = ROUND_DOWN(a + PAGE_SIZE * NPTENTRIES, PAGE_SIZE * NPTENTRIES);
      continue;
    }

    if (page != NULL)
      vm_remove_page(vm->trtab, a);

    a += PAGE_SIZE;
  }
}

/*
 * ----------------------------------------------------------------------------
 * Copying Data Between Address Spaces
 * ----------------------------------------------------------------------------
 */

int
vm_user_copy_out(struct VM *vm, void *dst_va, const void *src_va, size_t n)
{
  uint8_t *src = (uint8_t *) src_va;
  uint8_t *dst = (uint8_t *) dst_va;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    if ((page = vm_lookup_page(vm->trtab, dst, NULL)) == NULL)
      return -EFAULT;
    
    kva    = (uint8_t *) page2kva(page);
    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    memmove(kva + offset, src, ncopy);

    src += ncopy;
    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

int
vm_user_copy_in(struct VM *vm, void *dst_va, const void *src_va, size_t n)
{
  uint8_t *dst = (uint8_t *) dst_va;
  uint8_t *src = (uint8_t *) src_va;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    if ((page = vm_lookup_page(vm->trtab, src, NULL)) == NULL)
      return -EFAULT;

    kva    = (uint8_t *) page2kva(page);
    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    memmove(dst, kva + offset, ncopy);

    src += ncopy;
    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

/*
 * ----------------------------------------------------------------------------
 * Check User Memory Permissions
 * ----------------------------------------------------------------------------
 */

int
vm_user_check_buf(struct VM *vm, const void *va, size_t n, unsigned perm)
{
  struct Page *page, *new_page;
  const char *p, *end;
  pte_t *pte;

  p   = ROUND_DOWN((const char *) va, PAGE_SIZE);
  end = ROUND_UP((const char *) va + n, PAGE_SIZE);

  if ((p >= (char *) KERNEL_BASE) || (end > (char *) KERNEL_BASE))
    return -EFAULT;

  while (p != end) {
    unsigned curr_perm;

    if ((page = vm_lookup_page(vm->trtab, (void *) p, &pte)) == NULL)
      return -EFAULT;

    curr_perm = vm_pte_get_flags(pte);

    if ((perm & VM_WRITE) && (curr_perm & VM_COW)) {
      curr_perm &= ~VM_COW;
      curr_perm |= VM_WRITE;

      if ((curr_perm & perm) != perm)
        return -EFAULT;

      if ((new_page = page_alloc_one(0)) == NULL)
        return -EFAULT;

      memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);

      if ((vm_insert_page(vm->trtab, new_page, (void *) p, curr_perm)) != 0)
        return -EFAULT;
    } else {
      if ((curr_perm & perm) != perm)
        return -EFAULT;
    }

    p += PAGE_SIZE;
  }

  return 0;
}

int
vm_user_check_str(struct VM *vm, const char *s, unsigned perm)
{
  struct Page *page;
  const char *p;
  unsigned off;
  pte_t *pte;

  assert(KERNEL_BASE % PAGE_SIZE == 0);

  while (s < (char *) KERNEL_BASE) {
    page = vm_lookup_page(vm->trtab, s, &pte);
    if ((page == NULL) || ((*(pte + (NPTENTRIES * 2)) & perm) != perm))
      return -EFAULT;

    p = (const char *) page2kva(page);
    for (off = (uintptr_t) s % PAGE_SIZE; off < PAGE_SIZE; off++)
      if (p[off] == '\0')
        return 0;
    s += off;
  }

  return -EFAULT;
}

/*
 * ----------------------------------------------------------------------------
 * Loading Binaries
 * ----------------------------------------------------------------------------
 */

int
vm_user_load(struct VM *vm, void *va, struct Inode *ip, size_t n, off_t off)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    page = vm_lookup_page(vm->trtab, dst, NULL);
    if (page == NULL)
      return -EFAULT;

    kva = (uint8_t *) page2kva(page);

    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    if ((r = fs_inode_read(ip, kva + offset, ncopy, &off)) != ncopy)
      return r;

    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

struct VM   *
vm_create(void)
{
  struct VM *vm;
  struct Page *trtab_page;

  if ((vm = (struct VM *) kobject_alloc(vm_pool)) == NULL)
    return NULL;

  if ((trtab_page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL) {
    kobject_free(vm_pool, vm);
    return NULL;
  }

  trtab_page->ref_count++;

  vm->trtab = page2kva(trtab_page);
  list_init(&vm->areas);

  return vm;
}

void
vm_destroy(struct VM *vm)
{
  struct Page *page;
  unsigned i;

  vm_user_dealloc(vm, (void *) 0, KERNEL_BASE);

  for (i = 0; i < TTX(KERNEL_BASE); i += 2) {
    if (!vm->trtab[i])
      continue;

    page = pa2page(PTE_SMALL_ADDR(vm->trtab[i]));
    if (--page->ref_count == 0)
      page_free_one(page);
  }

  page = kva2page(vm->trtab);
  if (--page->ref_count == 0)
      page_free_one(page);

  kobject_free(vm_pool, vm);
}

struct VM   *
vm_clone(struct VM *vm)
{
  struct VM *new_vm;
  struct Page *src_page, *dst_page;
  uint8_t *va;
  pte_t *pte;

  if ((new_vm = vm_create()) == NULL)
    return NULL;

  va = (uint8_t *) 0;

  while (va < (uint8_t *) KERNEL_BASE) {
    src_page = vm_lookup_page(vm->trtab, va, &pte);
    if (pte == NULL) {
      // Skip the rest of the page table 
      va = ROUND_DOWN(va + PAGE_SIZE * NPTENTRIES, PAGE_SIZE * NPTENTRIES);
      continue;
    }

    if (src_page != NULL) {
      unsigned perm;

      perm = vm_pte_get_flags(pte);

      if ((perm & VM_WRITE) || (perm & VM_COW)) {
        perm &= ~VM_WRITE;
        perm |= VM_COW;

        if (vm_insert_page(vm->trtab, src_page, va, perm) < 0)
          panic("Cannot change page permissions");

        if (vm_insert_page(new_vm->trtab, src_page, va, perm) < 0) {
          vm_destroy(new_vm);
          return NULL;
        }
      } else {
        if ((dst_page = page_alloc_one(0)) == NULL) {
          vm_destroy(new_vm);
          return NULL;
        }

        if (vm_insert_page(new_vm->trtab, dst_page, va, perm) < 0) {
          page_free_one(dst_page);
          vm_destroy(new_vm);
          return NULL;
        }

        memcpy(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);
      }
    }

    va += PAGE_SIZE;
  }

  return new_vm;
}