#ifndef KERNEL_PAGE_H
#define KERNEL_PAGE_H

/**
 * @file kernel/page.h
 * 
 * Page allocator.
 */

#include <assert.h>
#include <list.h>

#include "memlayout.h"

/**
 * Physical page block info.
 */
struct PageInfo {
  struct ListLink link;             ///< Linked list head
  int             ref_count;        ///< Reference count to the page block
};

extern struct PageInfo *pages;
extern unsigned npages;

/**
 * Given a page info structure, return the starting physical address.
 * 
 * @param p Address of the page info structure.
 * @return The corresponding physical address.
 */
static inline physaddr_t
page2pa(struct PageInfo *p)
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
page2kva(struct PageInfo *p)
{
  return KADDR(page2pa(p));
}

/**
 * Given a physical address, return the page info structure.
 * 
 * @param pa The physical address.
 * @return The address of the corresponding page info structure.
 */
static inline struct PageInfo *
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
static inline struct PageInfo *
kva2page(void *va)
{
  return pa2page(PADDR(va));
}

/** The maximum page allocation order. */
#define PAGE_ORDER_MAX    10

/** Fill the allocated page block with zeros. */ 
#define PAGE_ALLOC_ZERO   (1 << 0)

void             page_init_low(void);
void             page_init_high(void);

struct PageInfo *page_alloc(int flags);
struct PageInfo *page_alloc_block(unsigned order, int flags);

void             page_free(struct PageInfo *page);
void             page_free_block(struct PageInfo *page, unsigned order);
void             page_free_region(physaddr_t start, physaddr_t end);

#endif  // !KERNEL_PAGE_H
