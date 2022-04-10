#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/sync.h>
#include <kernel/types.h>

#include <kernel/mm/page.h>

/** The kernel uses this array to keep track of physical pages. */
struct Page *pages;

/** The total number of physical pages in memory. */
unsigned npages;

// Page allocator implements the binary buddy algorithm.
//
// All physical memory is represented as a collection of blocks where each block
// is a power of 2 number of pages. The allocator maintains a list of the free
// blocks of each size.
//
// Allocation are always for a specified order (i.e. log2 of the desired block
// size). If no block of the requested order is available, a higher order block
// is split into two buddies. One is allocated and the other is placed to the
// appropriate free list.
//
// When the block is later freed, the allocator checks whether the buddy of the
// deallocated block is free, in which case two blocks are merged to form a
// higher order block and placed on the higher free list.
//
static struct {
  struct ListLink link;
  uint32_t       *bitmap;
} free_pages[PAGE_ORDER_MAX + 1];

static int pages_inited = 0;
static struct SpinLock pages_lock;

static void             page_mark_free(struct Page *, unsigned);
static void             page_mark_used(struct Page *, unsigned);
static int              page_is_free(struct Page *, unsigned);
static struct Page *page_split(struct Page *, unsigned, unsigned);

/*
 * ----------------------------------------------------------------------------
 * Initializing the page allocator
 * ----------------------------------------------------------------------------
 * 
 * Initialization happens in two phases:
 * 
 * 1. main() calls page_init_low() while still using the initial translation
 *    table to place just the pages mapped by entry_trtab on the free list.
 * 2. main() calls page_init_high() after installing the full kernel
 *    translation table to place the rest of the pages on the free list.
 *
 */

/**
 * Begin the page allocator initialization.
 */
void
page_init_low(void)
{
  unsigned i;
  size_t bitmap_len;

  spin_init(&pages_lock, "pages_lock");

  // TODO: detect the actual amount of physical memory!
  npages = PHYS_TOP / PAGE_SIZE;

  // Allocate the 'pages' array.
  pages = (struct Page *) boot_alloc(npages * sizeof(struct Page));

  // Initialize the free page lists.
  for (i = 0; i <= PAGE_ORDER_MAX; i++) {
    list_init(&free_pages[i].link);

    bitmap_len = ROUND_UP(npages / (1U << i), 8);
    free_pages[i].bitmap = (uint32_t *) boot_alloc(bitmap_len);
  }

  // Place pages mapped by 'entry_trtab' to the free list.
  page_free_region(0, KERNEL_LOAD);
  page_free_region(PADDR(boot_alloc(0)), PHYS_ENTRY_TOP);

  pages_inited = 1;
}

/**
 * Finish the page allocator initialization.
 */
void
page_init_high(void)
{
  page_free_region(PHYS_ENTRY_TOP, PHYS_TOP);
}

/*
 * ----------------------------------------------------------------------------
 * Boot-time memory allocator
 * ----------------------------------------------------------------------------
 * 
 * Simple memory allocator used only during the initialization of free page
 * block management structures.
 *
 */

void *
boot_alloc(size_t n)
{
  // First address after the kernel loaded from ELF file.
  extern uint8_t _end[];

  // Virtual address of the next byte of free memory.
  static uint8_t *nextptr = NULL;

  uint8_t *ret;

  if (pages_inited)
    panic("called after the page allocator is already initialized");

  // If this is the first time the allocator is invoked, initialize nextptr.
  if (nextptr == NULL)
    nextptr = ROUND_UP((uint8_t *) _end, sizeof(uintptr_t));

  ret = nextptr;

  // Allocate a chunk large enough to hold 'n' bytes and initialize it to 0.
  // Make sure nextptr is kept properly aligned.
  if (n != 0) {
    n = ROUND_UP(n, sizeof(uintptr_t));
    nextptr += n;

    if (PADDR(nextptr) > PHYS_ENTRY_TOP)
      panic("out of memory");

    memset(ret, 0, n);
  }

  return ret;
}

/*
 * ----------------------------------------------------------------------------
 * Allocating pages
 * ----------------------------------------------------------------------------
 */

/**
 * Allocate a single page.
 * 
 * @param flags The set of allocation flags.
 *
 * @return Address of a page info structure or NULL if out of memory.
 */
struct Page *
page_alloc(int flags)
{
  return page_alloc_block(0, flags);
}

/**
 * Allocate a block of '2^order' pages.
 * 
 * @param order The allocation order.
 * @param flags The set of allocation flags.
 *
 * @return Address of a page info structure or NULL if out of memory.
 */
struct Page *
page_alloc_block(unsigned order, int flags)
{
  struct ListLink *link;
  struct Page *page;
  unsigned curr_order;

  spin_lock(&pages_lock);

  for (curr_order = order; curr_order <= PAGE_ORDER_MAX; curr_order++) {
    if (list_empty(&free_pages[curr_order].link))
      continue;

    // If there is a free page block at cur_order, allocate it.
    link = free_pages[curr_order].link.next;
    page = LIST_CONTAINER(link, struct Page, link);
    page_mark_used(page, curr_order);

    page = page_split(page, curr_order, order);

    assert(page->ref_count == 0);
    assert(!page_is_free(page, order));

    spin_unlock(&pages_lock);

    if (flags & PAGE_ALLOC_ZERO) {
      memset(page2kva(page), 0, PAGE_SIZE << order);
    }

    return page;
  }

  spin_unlock(&pages_lock);

  return NULL;
}

// Split page block of order 'high' until a page block of order 'low' is
// available.
static struct Page *
page_split(struct Page *page, unsigned high, unsigned low)
{
  unsigned block_order, block_size;

  block_size  = (1U << high);
  block_order = high;

  assert((page - pages) % block_size == 0);
  assert(!page_is_free(page, high));

  while (block_order > low) {
    block_size >>= 1;
    block_order--;

    page_mark_free(page, block_order);

    page += block_size;
  }

  return page;
}

/*
 * ----------------------------------------------------------------------------
 * Free pages
 * ----------------------------------------------------------------------------
 */

/**
 * Free a single page.
 * 
 * @param page Address of the page info structure to be freed.
 */
void
page_free(struct Page *page)
{
  page_free_block(page, 0);
}

/**
 * Free a block of 2^order pages.
 * 
 * @param page  Address of the page info structure to be freed.
 * @param order The order of the page block.
 */
void
page_free_block(struct Page *page, unsigned order)
{
  struct Page *buddy;
  unsigned curr_order, pgnum, mask;

  if (page->ref_count != 0)
    panic("ref_count is not zero");

  pgnum = page - pages;
  mask = (1U << order);

  assert(pgnum % mask == 0);

  spin_lock(&pages_lock);

  for (curr_order = order; curr_order < PAGE_ORDER_MAX; curr_order++) {
    buddy = &pages[pgnum ^ mask];

    if (!page_is_free(buddy, curr_order))
      break;

    page_mark_used(buddy, curr_order);

    pgnum &= ~mask;
    mask <<= 1;
  }

  page_mark_free(&pages[pgnum], curr_order);
  
  spin_unlock(&pages_lock);
}

/**
 * Free the specified physical memory range to the page allocator.
 *
 * @param start The starting physical address.
 * @param end   The ending physial address.
 */
void
page_free_region(physaddr_t start, physaddr_t end)
{
  unsigned block_order, block_size, curr_pfn, last_pfn; 

  curr_pfn = (start + PAGE_SIZE - 1) / PAGE_SIZE;
  last_pfn = (end / PAGE_SIZE);

  while (curr_pfn < last_pfn) {
    block_order = PAGE_ORDER_MAX;
    block_size  = (1U << block_order);

    // Determine the maximum order for this page frame number.
    while (block_order > 0) {
      if (!(curr_pfn & (block_size - 1)) && (curr_pfn + block_size <= last_pfn))
        break;

      block_size >>= 1;
      block_order--;
    }

    page_free_block(&pages[curr_pfn], block_order);

    curr_pfn += block_size;
  }
}

/*
 * ----------------------------------------------------------------------------
 * Free list manipulation
 * ----------------------------------------------------------------------------
 */

// Check whether the page block 'p' of the given 'order' is free.
static int
page_is_free(struct Page *p, unsigned order)
{
  unsigned blockno;

  blockno = (p - pages) / (1U << order);
  return free_pages[order].bitmap[blockno / 32] & (1U << (blockno % 32));
}

// Add the page block 'p' to the free list for 'order' and set the corresponding
// bit.
static void
page_mark_free(struct Page *p, unsigned order)
{
  unsigned pageno, blockno;

  pageno = p - pages;

  assert((pageno % (1U << order)) == 0);
  assert(!page_is_free(p, order));

  list_add_front(&free_pages[order].link, &p->link);

  blockno = pageno / (1 << order);
  free_pages[order].bitmap[blockno / 32] |= (1U << (blockno % 32));
}

// Remove the page block 'p' from the free list for 'order' and clear the
// corresponding bit.
static void
page_mark_used(struct Page *p, unsigned order)
{
  unsigned pageno, blockno;

  pageno = p - pages;

  assert((pageno % (1U << order)) == 0);
  assert(page_is_free(p, order));

  list_remove(&p->link);

  blockno = pageno / (1U << order);
  free_pages[order].bitmap[blockno / 32] &= ~(1U << (blockno % 32));
}
