#ifndef __INCLUDE_ARGENTUM_MM_PAGE_H__
#define __INCLUDE_ARGENTUM_MM_PAGE_H__

/**
 * @file include/argentum/mm/page.h
 * 
 * Physical page allocator.
 */

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <assert.h>
#include <stddef.h>

#include <argentum/list.h>
#include <argentum/mm/memlayout.h>

struct KMemSlab;

/**
 * Physical page block info.
 */
struct Page {
  struct ListLink     link;         ///< Link into the free list
  int                 ref_count;    ///< Reference counter
  struct KMemSlab *slab;         ///< The slab this page block belongs to
};

extern struct Page *pages;
extern unsigned pages_length;

/**
 * Given a page info structure, return the starting physical address.
 * 
 * @param p Address of the page info structure.
 * @return The corresponding physical address.
 */
static inline physaddr_t
page2pa(struct Page *p)
{
  if ((p < pages) || (p >= &pages[pages_length]))
    panic("invalid page index %u", (p - pages));

  return (p - pages) << PAGE_SHIFT;
}

/**
 * Given a page info structure, return the starting kernel virtual address.
 * 
 * @param p Address of the page info structure.
 * @return The corresponding kernel virtual address.
 */
static inline void *
page2kva(struct Page *p)
{
  return PA2KVA(page2pa(p));
}

/**
 * Given a physical address, return the page info structure.
 * 
 * @param pa The physical address.
 * @return The address of the corresponding page info structure.
 */
static inline struct Page *
pa2page(physaddr_t pa)
{
  if ((pa >> PAGE_SHIFT) >= pages_length)
    panic("invalid page index %u", pa >> PAGE_SHIFT);

  return &pages[pa >> PAGE_SHIFT];
}

/**
 * Given a page info structure, return the starting kernel virtual address.
 * 
 * @param p Address of the page info structure.
 * @return The corresponding kernel virtual address.
 */
static inline struct Page *
kva2page(void *va)
{
  return pa2page(KVA2PA(va));
}

/** The maximum page allocation order. */
#define PAGE_ORDER_MAX    10

/** Fill the allocated page block with zeros. */ 
#define PAGE_ALLOC_ZERO   (1 << 0)

void         page_init_low(void);
void         page_init_high(void);
struct Page *page_alloc_block(unsigned, int);
void         page_free_block(struct Page *, unsigned);
void         page_free_region(physaddr_t, physaddr_t);

/**
 * Allocate a single page.
 * 
 * @param flags Allocation flags.
 *
 * @return Address of a page info structure or NULL if out of memory.
 */
static inline struct Page *
page_alloc_one(int flags)
{
  return page_alloc_block(0, flags);
}

/**
 * Free a single page.
 * 
 * @param page Pointer to the page structure corresponding to the block.
 */
static inline void
page_free_one(struct Page *page)
{
  page_free_block(page, 0);
}

#endif  // !__INCLUDE_ARGENTUM_MM_PAGE_H__
