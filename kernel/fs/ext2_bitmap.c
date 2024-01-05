#include <kernel/assert.h>
#include <errno.h>

#include <kernel/fs/buf.h>
#include <kernel/types.h>

#include "ext2.h"

#define BITS_PER_BYTE   8
#define BITS_PER_WORD   (sizeof(uint32_t) * BITS_PER_BYTE)

static inline int
bit_test(uint32_t *bmap, size_t n)
{
  return bmap[n / BITS_PER_WORD] & (1U << (n % BITS_PER_WORD));
}

static inline void
bit_set(uint32_t *bmap, size_t n)
{
  bmap[n / BITS_PER_WORD] |= (1U << (n % BITS_PER_WORD)); 
}

static inline void
bit_clear(uint32_t *bmap, size_t n)
{
  bmap[n / BITS_PER_WORD] &= ~(1U << (n % BITS_PER_WORD)); 
}

/**
 * Try to allocate a bit from the bitmap.
 * 
 * @param bstart Starting block ID of the bitmap.
 * @param blen   The length of the bitmap (in bits).
 * @param dev    The device where the bitmap is located.
 * @param bstore Pointer to the memory location to store the allocated bit
 *               number of.
 * 
 * @retval 0       on success
 * @retval -ENOMEM if there are no unused bits
 */
int
ext2_bitmap_alloc(uint32_t bstart, size_t blen, dev_t dev, uint32_t *bstore)
{
  uint32_t bits_per_block = (1024U << ext2_sb.log_block_size) * BITS_PER_BYTE;
  uint32_t b, bi;

  for (b = 0; b < blen; b += bits_per_block) {
    struct Buf *buf;
    uint32_t *bmap;

    if ((buf = buf_read(bstart + b / bits_per_block, dev)) == NULL)
      // TODO: recover from I/O errors
      panic("cannot read the bitmap block %d", bstart + b / bits_per_block);

    bmap = (uint32_t *) buf->data;

    for (bi = 0; bi < MIN(bits_per_block, blen - b); bi++) {
      if (bit_test(bmap, bi))
        continue;

      bit_set(bmap, bi);
      buf->flags |= BUF_DIRTY;

      buf_release(buf);
      // TODO: recover from I/O errors

      *bstore = b + bi;

      return 0;
    }

    buf_release(buf);
  }

  return -ENOMEM;
}

/**
 * Free the allocated bit.
 * 
 * @param bstart Starting block number of the bitmap.
 * @param dev    The device where the bitmap is located.
 * @param bit_no The bit number to be freed.
 * 
 * @retval 0       on success
 */
int
ext2_bitmap_free(uint32_t bstart, dev_t dev, uint32_t bit_no)
{
  uint32_t bits_per_block = (1024U << ext2_sb.log_block_size) * BITS_PER_BYTE;
  uint32_t b, bi;
  struct Buf *buf;
  uint32_t *bmap;

  b  = bit_no / bits_per_block;
  bi = bit_no % bits_per_block;

  if ((buf = buf_read(bstart + b, dev)) == NULL)
    // TODO: recover from I/O errors
    panic("cannot read the bitmap block %d", bstart + b);

  bmap = (uint32_t *) buf->data;

  if (!bit_test(bmap, bi))
    panic("bit not allocated");

  bit_clear(bmap, bi);
  buf->flags |= BUF_DIRTY;

  buf_release(buf);
  // TODO: recover from I/O errors

  return 0;
}
