#ifndef __KERNEL_MM_PAGE_H__
#define __KERNEL_MM_PAGE_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/page.h
 * 
 * Page allocator.
 */

#include <assert.h>
#include <stddef.h>

#include <kernel/list.h>
#include <kernel/mm/memlayout.h>

struct KObjectSlab;

/**
 * Physical page block info.
 */
struct Page {
  struct ListLink     link;         ///< Linked list node
  int                 ref_count;    ///< Reference counter
  struct KObjectSlab *slab;         ///< The slab this page belongs to
};

extern struct Page *pages;
extern unsigned npages;

/**
 * Given a page info structure, return the starting physical address.
 * 
 * @param p Address of the page info structure.
 * @return The corresponding physical address.
 */
static inline physaddr_t
page2pa(struct Page *p)
{
  assert((p >= pages) && (p < &pages[npages]));
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
  return KADDR(page2pa(p));
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
  assert((pa >> PAGE_SHIFT) < npages);
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
  return pa2page(PADDR(va));
}

/** The maximum page allocation order. */
#define PAGE_ORDER_MAX    10

/** Fill the allocated page block with zeros. */ 
#define PAGE_ALLOC_ZERO   (1 << 0)

void         page_init_low(void);
void         page_init_high(void);

struct Page *page_alloc_one(int);
struct Page *page_alloc_block(unsigned, int);

void         page_free_one(struct Page *);
void         page_free_block(struct Page *, unsigned);
void         page_free_region(physaddr_t, physaddr_t);

#endif  // !__KERNEL_MM_PAGE_H__
