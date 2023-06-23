#ifndef __KERNEL_INCLUDE_KERNEL_MM_VM_H__
#define __KERNEL_INCLUDE_KERNEL_MM_VM_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/list.h>
#include <kernel/armv7/mmu.h>

struct Inode;
struct Page;

/** Virtual memory mapping flags */
enum {
  /** Readable */
  VM_READ      = (1 << 0),
  /** Writeable */
  VM_WRITE     = (1 << 1),
  /** Accessible from user mode */
  VM_USER      = (1 << 2),
  /** Executable */
  VM_EXEC      = (1 << 3),
  /** Disable cacheing */
  VM_NOCACHE   = (1 << 4),
  /** Copy-on-write */
  VM_COW       = (1 << 5),
  /** Anonymous mapping (i.e. not a file or fixed physical address) */
  VM_ANONYMOUS = (1 << 6),
};

struct VMSpaceMapEntry {
  struct ListLink link;
  uintptr_t       start;
  size_t          length;
  int             flags;
};

struct VMSpace {
  l1_desc_t      *pgdir;
  struct ListLink areas;
};

void         vm_init(void);

struct Page *vm_page_lookup(void *, uintptr_t, int *);
int          vm_page_insert(void *, struct Page *, uintptr_t, int);
int          vm_page_remove(void *, uintptr_t);

struct VMSpace   *vm_space_create(void);
void         vm_space_destroy(struct VMSpace *);
struct VMSpace   *vm_space_clone(struct VMSpace *);

int          vm_space_copy_out(struct VMSpace *, void *, const void *, size_t);
int          vm_space_copy_in(struct VMSpace *, void *, const void *, size_t);
int          vm_space_check_buf(struct VMSpace *, const void *, size_t, unsigned);
int          vm_space_check_str(struct VMSpace *, const char *, unsigned);

int          vm_space_load_inode(struct VMSpace *, void *, struct Inode *, size_t, off_t);

int          vm_range_alloc(struct VMSpace *, void *, size_t, int);
void         vm_range_free(struct VMSpace *, void *, size_t);

int          vm_handle_fault(struct VMSpace *, uintptr_t);

// void         vm_print_areas(struct VMSpace *);
void        *vm_space_alloc(struct VMSpace *, void *, size_t, int);

#endif  // !__KERNEL_INCLUDE_KERNEL_MM_VM_H__
