#ifndef __KERNEL_MM_VM_H__
#define __KERNEL_MM_VM_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <armv7.h>
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
  l1_desc_t      *trtab;
  uintptr_t       heap;            ///< Heap end virtual address
  uintptr_t       stack;           ///< Stack bottom virtual address
  struct ListLink areas;
};

void         vm_init(void);

struct VM   *vm_create(void);
void         vm_destroy(struct VM *);
struct VM   *vm_clone(struct VM *);

int          vm_user_copy_out(struct VM *, void *, const void *, size_t);
int          vm_user_copy_in(struct VM *, void *, const void *, size_t);
int          vm_user_check_buf(struct VM *, const void *, size_t, unsigned);
int          vm_user_check_str(struct VM *, const char *, unsigned);

int          vm_user_load(struct VM *, void *, struct Inode *, size_t, off_t);

int          vm_user_alloc(struct VM *, void *, size_t, int);
void         vm_user_dealloc(struct VM *, void *, size_t);

int          vm_handle_fault(struct VM *vm, uintptr_t);

#endif  // !__KERNEL_MM_VM_H__
