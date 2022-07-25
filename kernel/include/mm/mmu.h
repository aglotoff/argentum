#ifndef __KERNEL_MMU_H__
#define __KERNEL_MMU_H__

#include <armv7.h>
#include <mm/memlayout.h>

l1_desc_t   *mmu_pgtab_create(void);
void         mmu_pgtab_destroy(l1_desc_t *);

l2_desc_t   *mmu_pte_get(l1_desc_t *, uintptr_t, int);

static inline int
mmu_pte_get_flags(l2_desc_t *pte)
{
  return *(pte + (L2_NR_ENTRIES * 2));
}

static inline void
mmu_pte_set_flags(l2_desc_t *pte, int flags)
{
  *(pte + (L2_NR_ENTRIES * 2)) = flags;
}

static inline int
mmu_pte_valid(l2_desc_t *pte)
{
  return (*pte & L2_DESC_TYPE_SM) == L2_DESC_TYPE_SM;
}

static inline physaddr_t
mmu_pte_base(l2_desc_t *pte)
{
  return L2_DESC_SM_BASE(*pte);
}

static inline void
mmu_invalidate_va(void *va)
{
  cp15_tlbimva((uintptr_t) va);
}

static inline void
mmu_pte_clear(l2_desc_t *pte)
{
  *pte = 0;
  mmu_pte_set_flags(pte, 0);
}

void         mmu_pte_set(l2_desc_t *, physaddr_t, int);

void         mmu_init(void);
void         mmu_init_percpu(void);

void         mmu_switch_kernel(void);
void         mmu_switch_user(l1_desc_t *);

#endif  // !__KERNEL_MMU_H__
