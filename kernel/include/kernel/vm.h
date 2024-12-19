#ifndef __KERNEL_VM_H__
#define __KERNEL_VM_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/mman.h>

#include <kernel/mm/memlayout.h>
#include <kernel/types.h>

#define VM_NONE       PROT_NONE
#define VM_READ       PROT_READ
#define VM_WRITE      PROT_WRITE
#define VM_EXEC       PROT_EXEC
#define VM_NOCACHE    PROT_NOCACHE
#define VM_USER       (1 << 4)
#define VM_COW        (1 << 5)
#define VM_PAGE       (1 << 6)

struct Page;

void        *arch_vm_create(void);
void         arch_vm_destroy(void *);
void        *arch_vm_lookup(void *, uintptr_t, int);
int          arch_vm_pte_valid(void *);
physaddr_t   arch_vm_pte_addr(void *);
int          arch_vm_pte_flags(void *);
void         arch_vm_pte_set(void *, physaddr_t, int);
void         arch_vm_pte_clear(void *);
void         arch_vm_invalidate(uintptr_t);
void         arch_vm_init(void);
void         arch_vm_init_percpu(void);
void         arch_vm_load_kernel(void);
void         arch_vm_load(void *);

struct Page *vm_page_lookup(void *, uintptr_t, int *);
int          vm_page_insert(void *, struct Page *, uintptr_t, int);
int          vm_page_remove(void *, uintptr_t);

int          vm_user_alloc(void *, uintptr_t, size_t, int);
void         vm_user_free(void *, uintptr_t, size_t);
int          vm_user_clone(void *, void *, uintptr_t, size_t, int);

int          vm_copy_out(void *, const void *, uintptr_t, size_t);
int          vm_copy_in(void *, void *, uintptr_t, size_t);
int          vm_clear(void *, uintptr_t, size_t);

int          vm_user_check_str(void *, uintptr_t, size_t *, int);
int          vm_user_check_ptr(void *, uintptr_t, int);
int          vm_user_check_buf(void *, uintptr_t, size_t, int);
int          vm_user_check_args(void *, uintptr_t, size_t *, int);

int          vm_handle_fault(void *, uintptr_t);

#endif  // !__KERNEL_VM_H__
