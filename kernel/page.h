#ifndef KERNEL_PAGE_H
#define KERNEL_PAGE_H

/**
 * @file kernel/page.h
 * 
 * Binary buddy allocator.
 */

#include <assert.h>
#include <list.h>
#include <stdint.h>

#include "memlayout.h"

#define PAGE_SIZE         1024      ///< The number of bytes in a single page
#define PAGE_SHIFT        10        ///< log2 of PAGE_SIZE

/**
 * Physical page frame info.
 */
struct PageInfo {
  struct ListLink link;             ///< Linked list head
  int             ref_count;        ///< Reference count to the page
};

struct PageFreeList {
  struct ListLink link;
  uint32_t       *bitmap;
};

/** The kernel uses this array to keep track of physical pages. */
extern struct PageInfo *pages;

/** The number of physical pages in memory */
extern unsigned npages;

static inline uint32_t
page2pa(struct PageInfo *pp)
{
  if ((pp < pages) || (pp >= &pages[npages]))
    panic("page2pa called with invalid pp %p", pp);
  return (pp - pages) << PAGE_SHIFT;
}

static inline struct PageInfo *
pa2page(uint32_t pa)
{
  if ((pa >> PAGE_SHIFT) >= npages)
    panic("pa2page called with invalid pa %p", pa);
  return &pages[pa >> PAGE_SHIFT];
}

static inline void *
page2kva(struct PageInfo *pp)
{
  return KADDR(page2pa(pp));
}

static inline struct PageInfo *
kva2page(void *va)
{
  return pa2page(PADDR(va));
}

void page_init_low(void);

void page_init_high(void);

#define PAGE_ORDER_PGTAB  0
#define PAGE_ORDER_TRTAB  4
#define PAGE_ORDER_MAX    10

#define PAGE_ALLOC_ZERO   (1 << 0)

struct PageInfo *page_alloc(unsigned order, int flags);

void page_free(struct PageInfo *pp, unsigned order);

void page_decref(struct PageInfo *pp, unsigned order);

#endif  // !KERNEL_PAGE_H
