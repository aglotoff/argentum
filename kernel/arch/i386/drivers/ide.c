#include <kernel/core/assert.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/list.h>
#include <kernel/interrupt.h>
#include <kernel/dev.h>
#include <kernel/fs/buf.h>
#include <kernel/console.h>
#include <kernel/page.h>

#include <arch/trap.h>
#include <arch/i386/io.h>
#include <arch/i386/ide.h>
#include <kernel/vm.h>

uint16_t ide_io_base;
uint16_t ide_ctrl_base;
uint16_t ide_dma_base;

enum {
  ATA_REG_DATA      = 0x0,
  ATA_REG_FEATURES  = 0x1,
  ATA_REG_SECCOUNT0 = 0x2,
  ATA_REG_LBA0      = 0x3,
  ATA_REG_LBA1      = 0x4,
  ATA_REG_LBA2      = 0x5,
  ATA_REG_HDDEVSEL  = 0x6,
  ATA_REG_COMMAND   = 0x7,
  ATA_REG_STATUS    = 0x7,

  ATA_REG_ALTSTATUS = 0xC,
  ATA_REG_CONTROL   = 0xC,
};

static uint8_t
ide_reg_read(uint8_t reg)
{
  if (reg >= ATA_REG_CONTROL)
    return inb(ide_ctrl_base + reg - ATA_REG_CONTROL);
  return inb(ide_io_base + reg);
}

static void
ide_reg_write(uint8_t reg, uint8_t data)
{
  if (reg >= ATA_REG_CONTROL)
    return outb(ide_ctrl_base + reg - ATA_REG_CONTROL, data);
  return outb(ide_io_base + reg, data);
}

enum {
  ATA_SR_BSY  = (1 << 7), // Budy
  ATA_SR_DRDY = (1 << 6), // Drive ready
  ATA_SR_DF   = (1 << 5), // Drive write fault
  ATA_SR_ERR  = (1 << 0), // Error
};

enum {
  ATA_CMD_READ_PIO  = 0x20,
  ATA_CMD_WRITE_PIO = 0x30,
  ATA_CMD_RDMUL     = 0xc4,
  ATA_CMD_WRMUL     = 0xc5,
  ATA_CMD_READ_DMA  = 0xc8,
  ATA_CMD_WRITE_DMA = 0xca,
  ATA_CMD_IDENTIFY  = 0xec,
};

struct KListLink ide_queue;
struct KMutex ide_mutex;

static void ide_irq_task(int, void *);
static void ide_start_transfer(struct Buf *);

struct PRD {
  uint32_t address;
  uint16_t count;
  uint16_t zero;
} __attribute__((packed));

static struct PRD *prd;

// Wait for IDE disk to become ready.
static int
ide_wait(int checkerr)
{
  int r;

  //for (i = 0; i < 4; i++)
  //  ide_reg_read(ATA_REG_ALTSTATUS);

  while (((r = ide_reg_read(ATA_REG_STATUS)) & (ATA_SR_BSY | ATA_SR_DRDY)) != ATA_SR_DRDY)
    ;

  if (checkerr && (r & (ATA_SR_DF | ATA_SR_ERR)) != 0)
    return -1;

  return 0;
}

struct BlockDev storage_dev = {
  .request = ide_request,
};

#define BM_STATUS_ACTIVE	0x01	/* active */
#define BM_STATUS_ERROR		0x02	/* error */
#define BM_STATUS_INTR		0x04	/* IDE interrupt */
#define BM_STATUS_DRVDMA	0x20

int
ide_init(uint32_t bar0, uint32_t bar1, uint32_t bar2, uint32_t bar3, uint32_t bar4)
{
  struct Page *prd_page;
  //uint8_t buf[512];

  ide_io_base   = bar0 ? (bar0 & ~0x3U) : 0x1F0;
  ide_ctrl_base = bar1 ? (bar1 & ~0x3U) : 0x3F6;
  ide_dma_base  = bar4 & ~0x3U;

  (void) bar2;
  (void) bar3;

  k_list_init(&ide_queue);
  k_mutex_init(&ide_mutex, "ide_queue");

  if ((prd_page = page_alloc_one(PAGE_ALLOC_ZERO, 0)) == NULL)
    k_panic("cannot allocate PRD");
  
  prd = (struct PRD *) page2kva(prd_page);
  prd_page->ref_count++;

  // Disable interrupts
  ide_reg_write(ATA_REG_CONTROL, 2);

  // Select drive
  ide_reg_write(ATA_REG_HDDEVSEL, 0xe0 | (0<<4));

  //ide_reg_write(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

  // Check if disk 0 is present

  if (ide_reg_read(ATA_REG_STATUS) == 0)
    k_panic("no disk");

  //ide_wait(0);

  //insl(ide_io_base + ATA_REG_DATA, buf, 512 / 4);

  //outb(ide_dma_base + 0x2, BM_STATUS_DRVDMA);

  interrupt_attach_task(IRQ_ATA1, ide_irq_task, NULL);

  dev_register_block(0, &storage_dev);

  return 0;
}

volatile int sss;

#define IDE_BLOCK_LEN   512

void
ide_request(struct Buf *buf)
{
  if (buf->block_size % IDE_BLOCK_LEN != 0)
    k_panic("block size must be a multiple of %u", IDE_BLOCK_LEN);

  if (k_mutex_lock(&ide_mutex) < 0)
    k_panic("TODO: error");

  // Add buffer to the queue.
  k_list_add_back(&ide_queue, &buf->queue_link);

  // If the buffer is at the front of the queue, immediately send it to the
  // hardware.
  if (ide_queue.next == &buf->queue_link)
    ide_start_transfer(buf);

  // Wait for the R/W operation to finish.
  // TODO: deal with errors!
  while ((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID)
    k_condvar_wait(&buf->wait_cond, &ide_mutex);

  k_mutex_unlock(&ide_mutex);
}

volatile uint8_t *test = (uint8_t *) VIRT_KERNEL_BASE;

static void
ide_start_transfer(struct Buf *buf)
{
  size_t nsectors;

  k_assert(k_mutex_holding(&ide_mutex));
  k_assert(buf->queue_link.prev == &ide_queue);
  k_assert(buf->block_size % IDE_BLOCK_LEN == 0);

  // cprintf("[k] start %d\n", buf->block_no);

  nsectors = buf->block_size / IDE_BLOCK_LEN;

  int sector = buf->block_no * nsectors;
  
  if (buf->flags & BUF_DIRTY) {
  //   k_panic("TODO");

  //   // WRITING
  //   ide_wait(0);
  //   ide_reg_write(ATA_REG_CONTROL, 0);  // generate interrupt
  //  // ide_reg_write(ATA_REG_FEATURES, 0);
  //   ide_reg_write(ATA_REG_SECCOUNT0, nsectors);  // number of sectors
  //   ide_reg_write(ATA_REG_LBA0, sector & 0xff);
  //   ide_reg_write(ATA_REG_LBA1, (sector >> 8) & 0xff);
  //   ide_reg_write(ATA_REG_LBA2, (sector >> 16) & 0xff);
  //   ide_reg_write(ATA_REG_HDDEVSEL, 0xe0 | ((0) << 4) | ((sector>>24)&0x0f));

  //   ide_reg_write(ATA_REG_COMMAND, (nsectors == 1) ? ATA_CMD_WRITE_PIO : ATA_CMD_WRMUL);
  //   outsl(ide_io_base + ATA_REG_DATA, buf->data, buf->block_size / 4);
    outb(ide_dma_base + 0x0, 0);

    // Clean Error and Interrupt bits
    outb(ide_dma_base + 0x2, BM_STATUS_ERROR | BM_STATUS_INTR);

    // Prepare PRDT
    prd->address = KVA2PA(buf->data);
    prd->count = buf->block_size;
    prd->zero = 0x8000;

    outl(ide_dma_base + 0x4, KVA2PA(prd));
    outl(ide_dma_base + 0xC, KVA2PA(prd));

    ide_reg_write(ATA_REG_HDDEVSEL, 0xe0 | ((0) << 4) | ((sector>>24)&0x0f));

    ide_reg_write(ATA_REG_CONTROL, 0);  // generate interrupt
    ide_reg_write(ATA_REG_SECCOUNT0, nsectors);  // number of sectors
    ide_reg_write(ATA_REG_LBA0, sector & 0xff);
    ide_reg_write(ATA_REG_LBA1, (sector >> 8) & 0xff);
    ide_reg_write(ATA_REG_LBA2, (sector >> 16) & 0xff);
    
    // WRITING
    ide_reg_write(ATA_REG_COMMAND, ATA_CMD_WRITE_DMA);
    
    outb(ide_dma_base + 0x0, 0x1 | 0x00);
  } else {
    //ide_wait(0);

    outb(ide_dma_base + 0x0, 0);

    // Clean Error and Interrupt bits
    outb(ide_dma_base + 0x2, BM_STATUS_ERROR | BM_STATUS_INTR);

    // Prepare PRDT
    prd->address = KVA2PA(buf->data);
    prd->count = buf->block_size;
    prd->zero = 0x8000;

    //cprintf("sector %d %d, prd = %p\n", sector, nsectors, KVA2PA(prd));

    // Send physical PRDT address
    outl(ide_dma_base + 0x4, KVA2PA(prd));
    outl(ide_dma_base + 0xC, KVA2PA(prd));

    ide_reg_write(ATA_REG_HDDEVSEL, 0xe0 | ((0) << 4) | ((sector>>24)&0x0f));

    ide_reg_write(ATA_REG_CONTROL, 0);  // generate interrupt
    ide_reg_write(ATA_REG_SECCOUNT0, nsectors);  // number of sectors
    ide_reg_write(ATA_REG_LBA0, sector & 0xff);
    ide_reg_write(ATA_REG_LBA1, (sector >> 8) & 0xff);
    ide_reg_write(ATA_REG_LBA2, (sector >> 16) & 0xff);
    
    // READING
    ide_reg_write(ATA_REG_COMMAND, ATA_CMD_READ_DMA);
    
    outb(ide_dma_base + 0x0, 0x1 | 0x08);
  }
}

static void
ide_irq_task(int irq, void *arg)
{
  struct KListLink *link;
  struct Buf *buf, *next_buf;

  (void) irq;
  (void) arg;

  k_mutex_lock(&ide_mutex);

  if (k_list_is_empty(&ide_queue))
    k_panic("queue is empty");

  // Grab the first buffer in the queue to find out whether a read or write
  // operation is happening
  link = ide_queue.next;
  

  buf = KLIST_CONTAINER(link, struct Buf, queue_link);

  k_assert((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID);
  k_assert(buf->block_size % IDE_BLOCK_LEN == 0);

  // cprintf("[k] end %d\n", buf->block_no);

  if (buf->flags & BUF_DIRTY) {
    // WRITING
    ide_wait(0);
  
    inb(ide_dma_base + 0x2);
    outb(ide_dma_base + 0x0, 0);

    buf->flags &= ~BUF_DIRTY;
  } else {
    ide_wait(0);
  
    inb(ide_dma_base + 0x2);
    outb(ide_dma_base + 0x0, 0);

    buf->flags |= BUF_VALID;
  }
  
  k_list_remove(link);

  // TODO
  arch_interrupt_unmask(irq);

  if (!k_list_is_empty(&ide_queue)) {
    next_buf = KLIST_CONTAINER(ide_queue.next, struct Buf, queue_link);
    ide_start_transfer(next_buf);
  }

  k_condvar_broadcast(&buf->wait_cond);

  k_mutex_unlock(&ide_mutex);
}
