#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <stddef.h>

#include "mmu.h"

struct PageInfo;

/**
 * Initialize the kernel part of a translation table.
 */
void vm_init_kernel(void);

void *vm_map_mmio(uint32_t pa, size_t n);

struct PageInfo *vm_lookup_page(tte_t *trtab, void *va, pte_t **pte_store);

int vm_insert_page(tte_t *trtab, struct PageInfo *p, void *va, int perm);

void vm_remove_page(tte_t *trtab, void *va);

#endif  // !KERNEL_VM_H
