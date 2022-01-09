#include <string.h>

#include "buf.h"
#include "console.h"
#include "ext2.h"
#include "fs.h"

struct Ext2Superblock sb;

static void
fs_read_superblock(void)
{
  struct Buf *buf;

  buf = buf_read(1);
  memcpy(&sb, buf->data, sizeof(sb));
  buf_release(buf);

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          sb.block_count * BLOCK_SIZE / (1024 * 1024),
          sb.inodes_count, sb.block_count);
}

void
fs_init(void)
{
  fs_read_superblock();
}
