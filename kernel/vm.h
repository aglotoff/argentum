#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <stddef.h>
#include <sys/types.h>

#include "mmu.h"

struct Inode;
struct PageInfo;

void             vm_init(void);
void             vm_init_percpu(void);

struct PageInfo *vm_lookup_page(tte_t *, const void *, pte_t **);
int              vm_insert_page(tte_t *, struct PageInfo *, void *, int);
void             vm_remove_page(tte_t *, void *);

void             vm_switch_kernel(void);
void             vm_switch_user(tte_t *);

int              vm_alloc_region(tte_t *, void *, size_t);
int              vm_dealloc_region(tte_t *, void *, size_t);
void             vm_free(tte_t *);
tte_t           *vm_copy(tte_t *);
int              vm_copy_out(tte_t *, void *, const void *, size_t);
int              vm_copy_in(tte_t *, void *, const void *, size_t);
int              vm_check_user_ptr(tte_t *, const void *, size_t, unsigned);
int              vm_check_user_str(tte_t *, const char *, unsigned);
int              vm_load(tte_t *, void *, struct Inode *, size_t, off_t);

#endif  // !KERNEL_VM_H
