#include <kernel/assert.h>
#include <kernel/drivers/sd.h>
#include <kernel/drivers/pl180.h>
#include <kernel/fs/buf.h>
#include <kernel/interrupt.h>

/*******************************************************************************
 * SD Card Driver
 *
 * The driver keeps the list of pending buffer requests in a queue, processing
 * them one at a time.
 * 
 * For details on SD card programming, see "SD Specifications. Part 1. Physical
 * Layer Simplified Specification. Version 1.10".
 ******************************************************************************/

// SD Card bus commands
enum {
  CMD_GO_IDLE_STATE        = 0,
  CMD_ALL_SEND_CID         = 2,
  CMD_SEND_RELATIVE_ADDR   = 3,
  CMD_SELECT_CARD          = 7,
  CMD_SEND_IF_COND         = 8,
  CMD_STOP_TRANSMISSION    = 12,
  CMD_SET_BLOCKLEN         = 16,
  CMD_READ_SINGLE_BLOCK    = 17,
  CMD_READ_MULTIPLE_BLOCK  = 18,
  CMD_WRITE_BLOCK          = 24,
  CMD_WRITE_MULTIPLE_BLOCK = 25,
  CM_SD_SEND_OP_COND       = 41,
  CMD_APP                  = 55,
};

// OCR Register fields
enum {
  OCR_VDD_MASK = (0xFFFF << 8), // VDD Voltage Window bitmask
  OCR_BUSY     = (1 << 31),     // Card power up status bit
};

static int  sd_irq_thread(int, void *);
static void sd_start_transfer(struct SD *, struct Buf *);

int
sd_init(struct SD *sd, struct PL180 *mmci, int irq)
{
  uint32_t resp[4], rca;

  // Put each card into Idle State
  pl180_send_cmd(mmci, CMD_GO_IDLE_STATE, 0, 0, NULL);

  // Request the card to send its valid operation conditions
  do {
    pl180_send_cmd(mmci, CMD_APP, 0, SD_RESPONSE_R1, NULL) ;
    pl180_send_cmd(mmci, CM_SD_SEND_OP_COND, OCR_VDD_MASK, SD_RESPONSE_R3,
                   resp);
  } while (!(resp[0] & OCR_BUSY));

  // Get the unique card identification (CID) number
  pl180_send_cmd(mmci, CMD_ALL_SEND_CID, 0, SD_RESPONSE_R2, NULL);

  // Ask the card to publish a new relative card address (RCA), which
  // will be used to address the card later
  pl180_send_cmd(mmci, CMD_SEND_RELATIVE_ADDR, 0, SD_RESPONSE_R6, &rca);

  // Select the card and put it into the Transfer State
  pl180_send_cmd(mmci, CMD_SELECT_CARD, rca & 0xFFFF0000, SD_RESPONSE_R1B, NULL);

  // Set the block length (512 bytes) for all I/O operations
  pl180_send_cmd(mmci, CMD_SET_BLOCKLEN, SD_BLOCKLEN, SD_RESPONSE_R1, NULL);

  sd->mmci = mmci;

  // Initialize the buffer queue
  k_list_init(&sd->queue);
  k_spinlock_init(&sd->lock, "sd_queue");

  // Enable interrupts
  pl180_k_irq_enable(sd->mmci);
  
  interrupt_attach_thread(irq, sd_irq_thread, sd);

  return 0;
}

void
sd_request(struct SD *sd, struct Buf *buf)
{
  if (buf->block_size % SD_BLOCKLEN != 0)
    panic("block size must be a multiple of %u", SD_BLOCKLEN);

  k_spinlock_acquire(&sd->lock);

  // Add buffer to the queue.
  k_list_add_back(&sd->queue, &buf->queue_link);

  // If the buffer is at the front of the queue, immediately send it to the
  // hardware.
  if (sd->queue.next == &buf->queue_link)
    sd_start_transfer(sd, buf);

  // Wait for the R/W operation to finish.
  // TODO: deal with errors!
  while ((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID)
    k_waitqueue_sleep(&buf->wait_queue, &sd->lock);

  k_spinlock_release(&sd->lock);
}

// Send the data transfer request to the hardware.
static void
sd_start_transfer(struct SD *sd, struct Buf *buf)
{
  uint32_t cmd, arg;
  size_t nblocks;

  assert(k_spinlock_holding(&sd->lock));
  assert(buf->queue_link.prev == &sd->queue);
  assert(buf->block_size % SD_BLOCKLEN == 0);

  nblocks = buf->block_size / SD_BLOCKLEN;

  if (buf->flags & BUF_DIRTY) {
    pl180_begin_transfer(sd->mmci, buf->block_size, 0);
    cmd = (nblocks > 1) ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_BLOCK;
  } else {
    pl180_begin_transfer(sd->mmci, buf->block_size, 1);
    cmd = (nblocks > 1) ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK;
  }

  arg = buf->block_no * buf->block_size;

  if (pl180_send_cmd(sd->mmci, cmd, arg, SD_RESPONSE_R1, NULL) != 0)
    panic("error sending cmd %d, arg %d", cmd, arg);
}

// Handle the SD card interrupts. Complete the current data transfer operation
// and wake up the corresponding task.
static int
sd_irq_thread(int irq, void *arg)
{
  struct SD *sd = (struct SD *) arg;
  struct KListLink *link;
  struct Buf *buf, *next_buf;

  (void) irq;

  k_spinlock_acquire(&sd->lock);

  if (k_list_is_empty(&sd->queue))
    panic("queue is empty");

  // Grab the first buffer in the queue to find out whether a read or write
  // operation is happening
  link = sd->queue.next;
  k_list_remove(link);

  buf = KLIST_CONTAINER(link, struct Buf, queue_link);

  assert((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID);
  assert(buf->block_size % SD_BLOCKLEN == 0);

  // Transfer the data and update the corresponding buffer flags.
  if (buf->flags & BUF_DIRTY) {
    if (pl180_send_data(sd->mmci, buf->data, buf->block_size) != 0)
      panic("error writing block %d", buf->block_no);
    buf->flags &= ~BUF_DIRTY;
  } else {
    if (pl180_receive_data(sd->mmci, buf->data, buf->block_size) != 0)
      panic("error reading block %d", buf->block_no);
    buf->flags |= BUF_VALID;
  }

  // Multiple block transfers must be stopped manually by issuing CMD12.
  if (buf->block_size > SD_BLOCKLEN)
    pl180_send_cmd(sd->mmci, CMD_STOP_TRANSMISSION, 0, SD_RESPONSE_R1B, NULL);

  // Begin processing the next buffer in the queue.
  if (!k_list_is_empty(&sd->queue)) {
    next_buf = KLIST_CONTAINER(sd->queue.next, struct Buf, queue_link);
    sd_start_transfer(sd, next_buf);
  }

  // Resume the task waiting for the buf data.
  k_waitqueue_wakeup_all(&buf->wait_queue);

  k_spinlock_release(&sd->lock);

  return 1;
}
