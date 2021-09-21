#include <stddef.h>
#include <string.h>

#include "console.h"
#include "page.h"

struct PageInfo *pages;
unsigned npages;

static struct PageFreeList free_pages[PAGE_ORDER_MAX + 1];

static void            *page_boot_alloc(size_t);
static void             page_mark_free(struct PageInfo *, unsigned);
static void             page_mark_used(struct PageInfo *, unsigned);
static int              page_is_free(struct PageInfo *, unsigned);
static struct PageInfo *page_split(struct PageInfo *, unsigned, unsigned);
static void             page_init_region(uint32_t, uint32_t);

void
page_init_low(void)
{
  unsigned i;
  size_t bitmap_len;

  // TODO: detect the actual amount of physical memory!
  npages = PHYS_TOP / PAGE_SIZE;

  // Initialize the free page lists.
  for (i = 0; i <= PAGE_ORDER_MAX; i++) {
    // Initialize the list head.
    list_init(&free_pages[i].link);

    // Allocate the bitmap of correspnding length and initialize to 0.
    bitmap_len = ((npages / (1 << i)) + 7) & ~7;
    free_pages[i].bitmap = (uint32_t *) page_boot_alloc(bitmap_len);
    memset(free_pages[i].bitmap, 0, bitmap_len);
  }

  // Allocate the pages array and initialize each field to 0.
  pages = (struct PageInfo *) page_boot_alloc(npages * sizeof(struct PageInfo));
  memset(pages, 0, npages * sizeof(struct PageInfo));

  page_init_region(PADDR(page_boot_alloc(0)), PHYS_ENTRY_TOP);
}

void
page_init_high(void)
{
  page_init_region(PHYS_ENTRY_TOP, PHYS_TOP);
}

struct PageInfo *
page_alloc(unsigned order, int flags)
{
  struct ListLink *link;
  struct PageInfo *page;
  unsigned curr_order;

  // TODO: acquire the lock

  for (curr_order = order; curr_order <= PAGE_ORDER_MAX; curr_order++) {
    if (list_empty(&free_pages[curr_order].link))
      continue;

    // If there is a free page block at cur_order, allocate it.
    link = free_pages[curr_order].link.next;
    page = LIST_CONTAINER(link, struct PageInfo, link);
    page_mark_used(page, curr_order);

    page = page_split(page, curr_order, order);

    // TODO: release the lock

    if (flags & PAGE_ALLOC_ZERO) {
      memset(page2kva(page), 0, PAGE_SIZE << order);
    }

    return page;
  }

  // TODO: release the lock

  return NULL;
}

void
page_decref(struct PageInfo *page, unsigned order)
{
  if (--page->ref_count == 0)
    page_free(page, order);
}

void
page_free(struct PageInfo *page, unsigned order)
{
  struct PageInfo *buddy;
  unsigned curr_order, pgnum, mask;

  pgnum = page - pages;
  mask = (1U << order);

  for (curr_order = order; curr_order < PAGE_ORDER_MAX; curr_order++) {
    buddy = &pages[pgnum ^ mask];
    if (!page_is_free(buddy, order))
      break;

    page_mark_used(&pages[pgnum], order);

    pgnum &= ~mask;
    mask <<= 1;
  }

  page_mark_free(&pages[pgnum], curr_order);
}


static void *
page_boot_alloc(size_t n)
{
  extern uint8_t _end[];

  static uint8_t *nextptr = NULL;
  uint8_t *ret;

  if (nextptr == NULL)
    nextptr = (uint8_t *) (((uintptr_t) (_end) + 3) & ~3);

  ret = nextptr;
  if (n != 0) {
    nextptr += (n + 3) & ~3;

    if (PADDR(nextptr) > PHYS_ENTRY_TOP) {
      // BUG!
    }
  }

  return ret;
}

static void
page_mark_free(struct PageInfo *p, unsigned order)
{
  unsigned pgnum;

  assert((p - pages) % (1 << order) == 0);
  assert(!page_is_free(p, order));

  list_add_front(&free_pages[order].link, &p->link);

  pgnum = (p - pages) / (1 << order);
  free_pages[order].bitmap[pgnum / 32] |= (1 << (pgnum % 32));
}

static void
page_mark_used(struct PageInfo *p, unsigned order)
{
  unsigned pgnum;

  assert((p->link.next != NULL) && (p->link.prev != NULL));
  assert((p - pages) % (1 << order) == 0);
  assert(page_is_free(p, order));

  // Remove 
  list_remove(&p->link);

  pgnum = (p - pages) / (1 << order);
  free_pages[order].bitmap[pgnum / 32] &= ~(1 << (pgnum % 32));
}

static int
page_is_free(struct PageInfo *p, unsigned order)
{
  unsigned pgnum;

  pgnum = (p - pages) / (1 << order);
  return free_pages[order].bitmap[pgnum / 32] & (1 << (pgnum % 32));
}

static struct PageInfo *
page_split(struct PageInfo *page, unsigned high, unsigned low)
{
  unsigned curr_order, size;

  size = (1U << high);
  curr_order = high;

  assert((page - pages) % size == 0);
  assert(!page_is_free(page, high));

  while (curr_order > low) {
    size >>= 1;
    curr_order--;
    page_mark_free(page, curr_order);
    page += size;
  }

  return page;
}

static void
page_init_region(uint32_t start, uint32_t end)
{
  unsigned order, size;
  uint32_t curr_pfn, end_pfn; 

  curr_pfn = (start + PAGE_SIZE - 1) / PAGE_SIZE;
  end_pfn = (end / PAGE_SIZE);

  while (curr_pfn < end_pfn) {
    order = PAGE_ORDER_MAX;
    size = (1U << order);

    // Determine the maximum order for this page frame number.
    while (order > 0) {
      if (((curr_pfn & (size - 1)) == 0) && (curr_pfn + size <= end_pfn))
        break;

      size >>= 1;
      order--;
    }

    page_free(&pages[curr_pfn], order);

    curr_pfn += size;
  }
}
