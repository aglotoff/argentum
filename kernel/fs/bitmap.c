#include <assert.h>
#include <errno.h>

#include <argentum/fs/buf.h>
#include <argentum/fs/ext2.h>
#include <argentum/fs/fs.h>

// The number of bits per bitmap block
// TODO: what about different block sizes?
#define BITS_PER_BLOCK   (BLOCK_SIZE * 8)

/**
 * Try to allocate a bit from the bitmap.
 * 
 * @param bitmap Starting block number of the bitmap.
 * @param n      The length of the bitmap (in bits).
 * @param dev    The device where the bitmap is located.
 * @param bstore Pointer to the memory location to store the relative number of
 *               the allocated bit.
 * 
 * @return 0 on success, -ENOMEM if there are no unused bits.
 */
int
ext2_bitmap_alloc(uint32_t bitmap, size_t n, dev_t dev, uint32_t *bstore)
{
  uint32_t b, bi;

  for (b = 0; b < n; b += BITS_PER_BLOCK) {
    struct Buf *buf;
    uint32_t *bmap;

    if ((buf = buf_read(bitmap + b, dev)) == NULL)
      panic("cannot read the bitmap block");

    bmap = (uint32_t *) buf->data;

    for (bi = 0; bi < BITS_PER_BLOCK; bi++) {
      if (bmap[bi / 32] & (1 << (bi % 32)))
        continue;

      bmap[bi / 32] |= (1 << (bi % 32));  

      buf_write(buf);
      buf_release(buf);

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
 * @param bitmap Starting block number of the bitmap.
 * @param dev    The device where the bitmap is located.
 * @param bit_no The bit number to be freed.
 */
void
ext2_bitmap_free(uint32_t bitmap, dev_t dev, uint32_t bit_no)
{
  uint32_t b, bi;
  struct Buf *buf;
  uint32_t *map;

  b  = bit_no / BITS_PER_BLOCK;
  bi = bit_no % BITS_PER_BLOCK;

  if ((buf = buf_read(bitmap + b, dev)) == NULL)
    panic("cannot read the bitmap block");

  map = (uint32_t *) buf->data;

  if (!(map[bi / 32] & (1 << (bi % 32))))
    panic("not allocated");

  map[bi / 32] &= ~(1 << (bi % 32));

  buf_write(buf);
  buf_release(buf);
}
