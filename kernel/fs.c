#include <string.h>

#include "buf.h"
#include "console.h"
#include "ext2.h"
#include "fs.h"
#include "mci.h"

struct Ext2Superblock sb;

static void
fs_read_superblock(void)
{
  struct Buf buf;

  list_init(&buf.wait_queue.head);
  spin_init(&buf.lock, "buf");
  buf.flags = 0;
  buf.block_no = 1;

  mci_request(&buf);

  memcpy(&sb, buf.data, sizeof(sb));

  cprintf("Filesystem size = %dM, inodes_count = %d, block_count = %d\n",
          sb.block_count * BLOCK_SIZE / (1024 * 1024),
          sb.inodes_count, sb.block_count);
  cprintf("%x\n", buf.data[0x249]);
}

void
fs_init(void)
{
  fs_read_superblock();
}
