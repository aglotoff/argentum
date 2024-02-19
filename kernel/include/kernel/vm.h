#ifndef __KERNEL_VM_H__
#define __KERNEL_VM_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <kernel/mm/memlayout.h>
#include <kernel/types.h>

struct Page;

void        *vm_arch_create(void);
void         vm_arch_destroy(void *);
void        *vm_arch_lookup(void *, uintptr_t, int);
int          vm_arch_pte_valid(void *);
physaddr_t   vm_arch_pte_addr(void *);
int          vm_arch_pte_flags(void *);
void         vm_arch_pte_set(void *, physaddr_t, int);
void         vm_arch_pte_clear(void *);
void         vm_arch_invalidate(uintptr_t);
void         vm_arch_init(void);
void         vm_arch_init_percpu(void);
void         vm_arch_load_kernel(void);
void         vm_arch_load(void *);

struct Page *vm_page_lookup(void *, uintptr_t, int *);
int          vm_page_insert(void *, struct Page *, uintptr_t, int);
int          vm_page_remove(void *, uintptr_t);

int          vm_range_alloc(void *, uintptr_t, size_t, int);
void         vm_range_free(void *, uintptr_t, size_t);
int          vm_range_clone(void *, void *, uintptr_t, size_t, int);

int          vm_copy_out(void *, const void *, uintptr_t, size_t);
int          vm_copy_in(void *, void *, uintptr_t, size_t);

#endif  // !__KERNEL_VM_H__
