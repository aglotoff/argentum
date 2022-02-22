#ifndef __KERNEL_MM_VM_H__
#define __KERNEL_MM_VM_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <kernel/mm/mmu.h>
#include <kernel/elf.h>

struct Inode;
struct PageInfo;

#define VM_READ     (1 << 0)  ///< Readable
#define VM_WRITE    (1 << 1)  ///< Writeable
#define VM_USER     (1 << 2)  ///< Accessible from user mode
#define VM_EXEC     (1 << 3)  ///< Executable
#define VM_NOCACHE  (1 << 4)  ///< Disable caching
#define VM_COW      (1 << 5)  ///< Copy-on-write

struct UserVm {
  tte_t    *trtab;    ///< Translation table
  uintptr_t heap;     ///< Heap end
  uintptr_t stack;    ///< Stack bottom
};

static inline int
vm_pte_get_flags(pte_t *pte)
{
  return *(pte + (NPTENTRIES * 2));
}

static inline void
vm_pte_set_flags(pte_t *pte, int flags)
{
  *(pte + (NPTENTRIES * 2)) = flags;
}

void             vm_init(void);
void             vm_init_percpu(void);

struct PageInfo *vm_lookup_page(tte_t *, const void *, pte_t **);
int              vm_insert_page(tte_t *, struct PageInfo *, void *, unsigned);
void             vm_remove_page(tte_t *, void *);

void             vm_switch_kernel(void);
void             vm_switch_user(tte_t *);

int              vm_alloc_region(tte_t *, void *, size_t, int);
void             vm_dealloc_region(tte_t *, void *, size_t);
void             vm_free(tte_t *);
tte_t           *vm_copy(tte_t *);
int              vm_copy_out(tte_t *, void *, const void *, size_t);
int              vm_copy_in(tte_t *, void *, const void *, size_t);
int              vm_check_user_ptr(tte_t *, const void *, size_t, unsigned);
int              vm_check_user_str(tte_t *, const char *, unsigned);
int              vm_load(tte_t *, void *, struct Inode *, size_t, off_t);
int              vm_load_binary(struct UserVm *, const Elf32_Ehdr *);

#endif  // !__KERNEL_MM_VM_H__
