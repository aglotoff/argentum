#include <string.h>

#include <kernel/boot_mem.h>
#include <kernel/mm/memlayout.h>
#include <kernel/types.h>

static uint8_t *boot_mem_next;
static uint8_t *boot_mem_end;

void
boot_mem_init(void *start, void *end)
{
  boot_mem_next = ROUND_UP((uint8_t *) start, sizeof(uintptr_t));
  boot_mem_end = (uint8_t *) end;
}

void
boot_mem_fini(void)
{
  boot_mem_next = boot_mem_end = NULL;
}

/**
 * Simple boot-time memory allocator to solve the "chicken and egg" problem
 * during the page allocator initialization. The memory obtained via this
 * function is never freed.
 * 
 * @param n The number of bytes to allocate.
 * 
 * @return Pointer to the allocated block of memory.
 */
void *
boot_mem_alloc(size_t n)
{
  void *ret;

  if (boot_mem_next == NULL)
    k_panic("boot_mem unavailable");

  ret = boot_mem_next;

  // Allocate a chunk large enough to hold 'n' bytes and initialize it to 0.
  // Make sure next_free is kept properly aligned.
  if (n != 0) {
    n = ROUND_UP(n, sizeof(uintptr_t));
    boot_mem_next += n;

    if (boot_mem_next > boot_mem_end)
      k_panic("out of memory");

    memset(ret, 0, n);
  }

  return ret;
}
