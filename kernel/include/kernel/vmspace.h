#ifndef __KERNEL_INCLUDE_KERNEL_VMSPACE_H__
#define __KERNEL_INCLUDE_KERNEL_VMSPACE_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/list.h>
#include <kernel/mm/vm.h>
#include <kernel/armv7/mmu.h>

struct Inode;
struct Page;

struct VMSpaceMapEntry {
  struct ListLink link;
  uintptr_t       start;
  size_t          length;
  int             flags;
};

struct VMSpace {
  l1_desc_t      *pgdir;
  uintptr_t       heap;
  uintptr_t       stack;
  // struct ListLink areas;
};

void              vm_space_init(void);
struct VMSpace   *vm_space_create(void);
void              vm_space_destroy(struct VMSpace *);
struct VMSpace   *vm_space_clone(struct VMSpace *, int);
int               vm_space_check_buf(struct VMSpace *, const void *, size_t,
                                     unsigned);
int               vm_space_check_str(struct VMSpace *, const char *, unsigned);
int               vm_space_load_inode(struct VMSpace *, void *, struct Inode *,
                                      size_t, off_t);
int               vm_handle_fault(struct VMSpace *, uintptr_t);
// intptr_t          vm_space_alloc(struct VMSpace *, uintptr_t, size_t, int);
// void         vm_print_areas(struct VMSpace *);

#endif  // !__KERNEL_INCLUDE_KERNEL_VMSPACE_H__
