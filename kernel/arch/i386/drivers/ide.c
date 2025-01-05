#include <kernel/assert.h>
#include <kernel/spinlock.h>
#include <kernel/core/list.h>
#include <kernel/interrupt.h>
#include <kernel/dev.h>
#include <kernel/fs/buf.h>
#include <kernel/console.h>

#include <arch/trap.h>
#include <arch/i386/io.h>
#include <arch/i386/ide.h>

#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

struct KListLink ide_queue;
struct KSpinLock ide_lock;

static int  ide_irq_thread(int, void *);
static void ide_start_transfer(struct Buf *);

// Wait for IDE disk to become ready.
static int
ide_wait(int checkerr)
{
  int r;

  while (((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;

  if (checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;

  return 0;
}

struct BlockDev storage_dev = {
  .request = ide_request,
};

int
ide_init(void)
{
  int i, has_disk;

  k_list_init(&ide_queue);
  k_spinlock_init(&ide_lock, "ide_queue");

  ide_wait(0);

  // Check if disk 0 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for (i = 0; i < 1000; i++) {
    if (inb(0x1f7) != 0) {
      has_disk = 1;
      break;
    }
  }

  if (!has_disk)
    panic("no disk");

  interrupt_attach_thread(IRQ_ATA1, ide_irq_thread, NULL);

  dev_register_block(0, &storage_dev);

  return 0;
}

#define IDE_BLOCK_LEN   512

void
ide_request(struct Buf *buf)
{
  if (buf->block_size % IDE_BLOCK_LEN != 0)
    panic("block size must be a multiple of %u", IDE_BLOCK_LEN);

  k_spinlock_acquire(&ide_lock);

  // Add buffer to the queue.
  k_list_add_back(&ide_queue, &buf->queue_link);

  // If the buffer is at the front of the queue, immediately send it to the
  // hardware.
  if (ide_queue.next == &buf->queue_link)
    ide_start_transfer(buf);

  // Wait for the R/W operation to finish.
  // TODO: deal with errors!
  while ((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID)
    k_waitqueue_sleep(&buf->wait_queue, &ide_lock);

  k_spinlock_release(&ide_lock);
}

static void
ide_start_transfer(struct Buf *buf)
{
  size_t nsectors;

  assert(k_spinlock_holding(&ide_lock));
  assert(buf->queue_link.prev == &ide_queue);
  assert(buf->block_size % IDE_BLOCK_LEN == 0);

  nsectors = buf->block_size / IDE_BLOCK_LEN;
  //if (nsectors > 7) panic("too many blocks");

  int sector = buf->block_no * nsectors;
  int read_cmd = (nsectors == 1) ? IDE_CMD_READ : IDE_CMD_RDMUL;
  int write_cmd = (nsectors == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  ide_wait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, nsectors);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((1) << 4) | ((sector>>24)&0x0f));

  if (buf->flags & BUF_DIRTY) {
    outb(0x1f7, write_cmd);
    outsl(0x1f0, buf->data, buf->block_size / 4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

static int
ide_irq_thread(int irq, void *arg)
{
  struct KListLink *link;
  struct Buf *buf, *next_buf;

  (void) irq;
  (void) arg;

  k_spinlock_acquire(&ide_lock);

  if (k_list_is_empty(&ide_queue))
    panic("queue is empty");

  // Grab the first buffer in the queue to find out whether a read or write
  // operation is happening
  link = ide_queue.next;
  k_list_remove(link);

  buf = KLIST_CONTAINER(link, struct Buf, queue_link);

  assert((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID);
  assert(buf->block_size % IDE_BLOCK_LEN == 0);


  if (buf->flags & BUF_DIRTY) {
    // TODO

    buf->flags &= ~BUF_DIRTY;
  } else {
    if (ide_wait(1) >= 0)
      insl(0x1f0, buf->data, buf->block_size/4);

    buf->flags |= BUF_VALID;
  }

  // TODO

  if (!k_list_is_empty(&ide_queue)) {
    next_buf = KLIST_CONTAINER(ide_queue.next, struct Buf, queue_link);
    ide_start_transfer(next_buf);
  }

  k_waitqueue_wakeup_all(&buf->wait_queue);

  k_spinlock_release(&ide_lock);

  return 1;
}
