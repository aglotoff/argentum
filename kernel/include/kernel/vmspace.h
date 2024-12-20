#ifndef __KERNEL_INCLUDE_KERNEL_VMSPACE_H__
#define __KERNEL_INCLUDE_KERNEL_VMSPACE_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/core/list.h>
#include <kernel/vm.h>
#include <kernel/spinlock.h>

struct Inode;
struct Page;

struct VMSpaceMapEntry {
  struct KListLink link;
  uintptr_t       start;
  size_t          length;
  int             flags;
};

struct VMSpace {
  void            *pgtab;
  struct KSpinLock lock;
  struct KListLink areas;
};

void              vm_space_init(void);
struct VMSpace   *vm_space_create(void);
void              vm_space_destroy(struct VMSpace *);
struct VMSpace   *vm_space_clone(struct VMSpace *, int);
int               vm_space_load_inode(void *, void *, struct Inode *,
                                      size_t, off_t);

intptr_t          vmspace_map(struct VMSpace *, uintptr_t, size_t, int);
void              vm_print_areas(struct VMSpace *);

int               vm_space_copy_out(const void *, uintptr_t, size_t);
int               vm_space_copy_in(void *, uintptr_t, size_t);
int               vm_space_clear(uintptr_t, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_VMSPACE_H__
