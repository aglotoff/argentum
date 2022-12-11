#include <string.h>

#include <argentum/cprintf.h>
#include <argentum/mm/page.h>
#include <argentum/spin.h>
#include <argentum/types.h>

/** The kernel uses this array to keep track of physical pages. */
struct Page *pages;

/** The maximum number of available physical pages. */
unsigned pages_length;

//
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
} free_list[PAGE_ORDER_MAX + 1];

static int pages_inited = 0;
static struct SpinLock page_lock;

static void        *boot_alloc(size_t);
static struct Page *page_buddy(struct Page *, unsigned);
static void         page_list_add(struct Page *, unsigned);
static void         page_list_remove(struct Page *, unsigned);
static int          page_list_contains(struct Page *, unsigned);

//
// Initialization happens in two phases:
// 
// 1. main() calls page_init_low() while still using the initial translation
//    table to place just the pages mapped by entry_trtab on the free list.
// 2. main() calls page_init_high() after installing the full kernel
//    translation table to place the rest of the pages on the free list.
//

/**
 * Begin the page allocator initialization.
 */
void
page_init_low(void)
{
  unsigned i;
  size_t bitmap_len;

  spin_init(&page_lock, "page_lock");

  // TODO: detect the actual amount of physical memory!
  pages_length = PHYS_LIMIT / PAGE_SIZE;

  // Allocate the 'pages' array.
  pages = (struct Page *) boot_alloc(pages_length * sizeof(struct Page));

  // Initialize the free page lists.
  for (i = 0; i <= PAGE_ORDER_MAX; i++) {
    list_init(&free_list[i].link);

    bitmap_len = ROUND_UP(pages_length / (1U << i), 8);
    free_list[i].bitmap = (uint32_t *) boot_alloc(bitmap_len);
  }

  // Place pages mapped by 'entry_trtab' to the free list.
  page_free_region(0, PHYS_KERNEL_LOAD);
  page_free_region(KVA2PA(boot_alloc(0)), PHYS_ENTRY_LIMIT);

  pages_inited = 1;
}

/**
 * Finish the page allocator initialization.
 */
void
page_init_high(void)
{
  page_free_region(PHYS_ENTRY_LIMIT, PHYS_LIMIT);
}

/**
 * Boot-time memory allocator.
 * 
 * Simple memory allocator used only during the initialization of free page
 * block management structures.
 * 
 * @param n The number of bytes to allocate.
 * 
 * @return Pointer to the allocated block of memory.
 */
static void *
boot_alloc(size_t n)
{
  // First address after the kernel loaded from ELF file.
  extern uint8_t _end[];
  // Virtual address of the next byte of free memory.
  static uint8_t *next_free = NULL;

  void *ret;

  if (pages_inited)
    panic("called after the page allocator is already initialized");

  // If this is the first time the allocator is invoked, initialize next_free.
  if (next_free == NULL)
    next_free = ROUND_UP((uint8_t *) _end, sizeof(uintptr_t));

  ret = next_free;

  // Allocate a chunk large enough to hold 'n' bytes and initialize it to 0.
  // Make sure next_free is kept properly aligned.
  if (n != 0) {
    n = ROUND_UP(n, sizeof(uintptr_t));
    next_free += n;

    if (KVA2PA(next_free) > PHYS_ENTRY_LIMIT)
      panic("out of memory");

    memset(ret, 0, n);
  }

  return ret;
}

/**
 * Allocate a block of the given order.
 * 
 * @param order The allocation order.
 * @param flags Allocation flags.
 *
 * @return Pointer to a page structure or NULL if out of memory.
 */
struct Page *
page_alloc_block(unsigned order, int flags)
{
  struct Page *page;
  unsigned curr_order;

  spin_lock(&page_lock);

  for (curr_order = order; curr_order <= PAGE_ORDER_MAX; curr_order++)
    if (!list_empty(&free_list[curr_order].link))
      break;

  if (curr_order > PAGE_ORDER_MAX) {
    spin_unlock(&page_lock);
    return NULL;
  }

  page = LIST_CONTAINER(free_list[curr_order].link.next, struct Page, link);

  page_list_remove(page, curr_order);

  // Split
  while (curr_order > order) {
    curr_order--;
    page_list_add(page, curr_order);
    page += (1U << curr_order);
  }

  assert(page->ref_count == 0);
  assert(!page_list_contains(page, order));

  spin_unlock(&page_lock);

  if (flags & PAGE_ALLOC_ZERO)
    memset(page2kva(page), 0, PAGE_SIZE << order);

  return page;
}

/**
 * Free a block of pages.
 * 
 * @param page  Pointer to the page structure corresponding to the block.
 * @param order The order of the page block.
 */
void
page_free_block(struct Page *page, unsigned order)
{
  struct Page *buddy;

  if (page->ref_count != 0)
    panic("page->ref_count != 0 (%u)", page->ref_count);

  spin_lock(&page_lock);

  for ( ; order < PAGE_ORDER_MAX; order++) {
    buddy = page_buddy(page, order);

    if (!page_list_contains(buddy, order))
      break;

    // Combine with buddy
    page_list_remove(buddy, order);
    if (buddy < page)
      page = buddy;
  }

  page_list_add(page, order);

  spin_unlock(&page_lock);
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
  unsigned blk_order, blk_length;
  unsigned page_idx, last_page_idx; 

  page_idx = (start + PAGE_SIZE - 1) / PAGE_SIZE;
  last_page_idx = (end / PAGE_SIZE);

  while (page_idx < last_page_idx) {
    blk_order  = PAGE_ORDER_MAX;
    blk_length = (1U << blk_order);

    // Determine the maximum order for this page index.
    while (blk_order > 0) {
      if (!(page_idx & (blk_length - 1)) &&
          (page_idx + blk_length <= last_page_idx))
        break;

      blk_length >>= 1;
      blk_order--;
    }

    page_free_block(&pages[page_idx], blk_order);

    page_idx += blk_length;
  }
}

/**
 * Get buddy of a page block.
 * 
 * @param page  Pointer to the page structure corresponding to the block.
 * @param order The page block order.
 * 
 * @return Pointer to the page structure of the buddy block.
 */
static struct Page *
page_buddy(struct Page *page, unsigned order)
{
  return &pages[(page - pages) ^ (1U << order)];
}

#define BITS_PER_WORD     (sizeof(uint32_t) * 8)
#define BITMAP_OFFSET(n)  ((n) / BITS_PER_WORD)
#define BITMAP_SHIFT(n)   ((n) % BITS_PER_WORD)
#define BITMAP_MASK(n)    (1U << BITMAP_SHIFT(n))

/**
 * Check whether a page block is available.
 * 
 * @param page  Pointer to the page structure corresponding to the block.
 * @param order The order of the page block.
 * 
 * @return A non-zero value if the page block is available, zero otherwise.
 */
static int
page_list_contains(struct Page *page, unsigned order)
{
  unsigned idx;

  idx = (page - pages) / (1U << order);
  return free_list[order].bitmap[BITMAP_OFFSET(idx)] & BITMAP_MASK(idx);
}

/**
 * Add page block to the free list of corresponding size.
 *
 * @param page  Pointer to the page structure corresponding to the block.
 * @param order The block order.
 */
static void
page_list_add(struct Page *page, unsigned order)
{
  unsigned block_idx, map_idx;

  block_idx = page - pages;

  assert((block_idx % (1U << order)) == 0);
  assert(!page_list_contains(page, order));

  list_add_front(&free_list[order].link, &page->link);

  map_idx = block_idx / (1 << order);
  free_list[order].bitmap[BITMAP_OFFSET(map_idx)] |= BITMAP_MASK(map_idx);
}

/**
 * Remove page block from the containing free list.
 * 
 * @param page  Pointer to the page structure corresponding to the block.
 * @param order The block order.
 */
static void
page_list_remove(struct Page *page, unsigned order)
{
  unsigned block_idx, map_idx;

  block_idx = page - pages;

  assert((block_idx % (1U << order)) == 0);
  assert(page_list_contains(page, order));

  list_remove(&page->link);

  map_idx = block_idx / (1U << order);
  free_list[order].bitmap[BITMAP_OFFSET(map_idx)] &= ~BITMAP_MASK(map_idx);
}
