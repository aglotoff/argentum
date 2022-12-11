#include <assert.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <argentum/cprintf.h>
#include <argentum/fs/buf.h>
#include <argentum/fs/ext2.h>
#include <argentum/fs/fs.h>
#include <argentum/types.h>

/*
 * ----------------------------------------------------------------------------
 * Superblock operations
 * ----------------------------------------------------------------------------
 */

struct Ext2Superblock sb;

void
ext2_read_superblock(void)
{
  struct Buf *buf;

  if ((buf = buf_read(1, 0)) == NULL)
    panic("cannot read the superblock");

  memcpy(&sb, buf->data, sizeof(sb));
  buf_release(buf);

  if (sb.log_block_size != 0)
    panic("block size must be 1024 bytes");

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          sb.block_count * BLOCK_SIZE / (1024 * 1024),
          sb.inodes_count, sb.block_count);
}
