#ifndef __KERNEL_MM_VM_H__
#define __KERNEL_MM_VM_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <mm/mmu.h>
#include <elf.h>
#include <list.h>

struct Inode;
struct Page;

#define VM_READ     (1 << 0)  ///< Readable
#define VM_WRITE    (1 << 1)  ///< Writeable
#define VM_USER     (1 << 2)  ///< Accessible from user mode
#define VM_EXEC     (1 << 3)  ///< Executable
#define VM_NOCACHE  (1 << 4)  ///< Disable caching
#define VM_COW      (1 << 5)  ///< Copy-on-write

struct VMArea {
  struct ListLink link;
  uintptr_t       start;
  size_t          length;
  int             flags;
};

struct VM {
  tte_t          *trtab;
  struct ListLink areas;
};

static inline int
vm_pte_get_flags(pte_t *pte)
{
  return *(pte + (NPTENTRIES * 2));
}

void         vm_init(void);
void         vm_init_percpu(void);

void         vm_switch_kernel(void);
void         vm_switch_user(struct VM *);

struct VM   *vm_create(void);
void         vm_destroy(struct VM *);
struct VM   *vm_clone(struct VM *);

struct Page *vm_lookup_page(tte_t *, const void *, pte_t **);
int          vm_insert_page(tte_t *, struct Page *, void *, unsigned);
void         vm_remove_page(tte_t *, void *);

int          vm_user_copy_out(struct VM *, void *, const void *, size_t);
int          vm_user_copy_in(struct VM *, void *, const void *, size_t);
int          vm_user_check_buf(struct VM *, const void *, size_t, unsigned);
int          vm_user_check_str(struct VM *, const char *, unsigned);

int          vm_user_load(struct VM *, void *, struct Inode *, size_t, off_t);

int          vm_user_alloc(struct VM *, void *, size_t, int);
void         vm_user_dealloc(struct VM *, void *, size_t);

#endif  // !__KERNEL_MM_VM_H__
