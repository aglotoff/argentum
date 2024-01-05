#include <kernel/assert.h>
#include <errno.h>
#include <stdint.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/fs/buf.h>
#include <kernel/irq.h>
#include <kernel/mm/memlayout.h>
#include <kernel/task.h>
#include <kernel/sd.h>
#include <kernel/spinlock.h>

#include "pl180.h"

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

static struct PL180 mmci;

// The queue of pending buffer requests
static struct {
  struct ListLink head;
  struct SpinLock lock;
} sd_queue;

static void sd_irq(void);
static void sd_start_transfer(struct Buf *);

/**
 * Initialize the SD card driver.
 * 
 * @return 0 on success, a non-zero value on error.
 */
int
sd_init(void)
{
  uint32_t resp[4], rca;

  // Power on.
  pl180_init(&mmci, PA2KVA(PHYS_MMCI));

  // Put each card into Idle State.
  pl180_send_cmd(&mmci, CMD_GO_IDLE_STATE, 0, 0, NULL);

  // Request the card to send its valid operation conditions.
  do {
    pl180_send_cmd(&mmci, CMD_APP, 0, SD_RESPONSE_R1, NULL) ;
    pl180_send_cmd(&mmci, CM_SD_SEND_OP_COND, OCR_VDD_MASK, SD_RESPONSE_R3,
                   resp);
  } while (!(resp[0] & OCR_BUSY));

  // Get the unique card identification (CID) number.
  pl180_send_cmd(&mmci, CMD_ALL_SEND_CID, 0, SD_RESPONSE_R2, NULL);

  // Ask the card to publish a new relative card address (RCA), which
  // will be used to address the card later.
  pl180_send_cmd(&mmci, CMD_SEND_RELATIVE_ADDR, 0, SD_RESPONSE_R6, &rca);

  // Select the card and put it into the Transfer State.
  pl180_send_cmd(&mmci, CMD_SELECT_CARD, rca & 0xFFFF0000, SD_RESPONSE_R1B, NULL);

  // Set the block length (512 bytes) for all I/O operations.
  pl180_send_cmd(&mmci, CMD_SET_BLOCKLEN, SD_BLOCKLEN, SD_RESPONSE_R1, NULL);

  // Initialize the buffer queue.
  list_init(&sd_queue.head);
  spin_init(&sd_queue.lock, "sd_queue");

  // Enable interrupts.
  pl180_cpu_irq_enable(&mmci);
  irq_attach(IRQ_MCIA, sd_irq, 0);

  return 0;
}

/**
 * Add buffer to the request queue and put the current process to sleep until
 * the operation is completed.
 * 
 * @param buf The buffer to be processed.
 */
void
sd_request(struct Buf *buf)
{
  if (!kmutex_holding(&buf->mutex))
    panic("buf not locked");
  if ((buf->flags & (BUF_DIRTY | BUF_VALID)) == BUF_VALID)
    panic("nothing to do");
  if (buf->dev != 0)
    panic("dev must be 0");
  if (buf->block_size % SD_BLOCKLEN != 0)
    panic("block size must be a multiple of %u", SD_BLOCKLEN);

  spin_lock(&sd_queue.lock);

  // Add buffer to the queue.
  list_add_back(&sd_queue.head, &buf->queue_link);

  // If the buffer is at the front of the queue, immediately send it to the
  // hardware.
  if (sd_queue.head.next == &buf->queue_link)
    sd_start_transfer(buf);

  // Wait for the R/W operation to finish.
  while ((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID)
    wchan_sleep(&buf->wait_queue, &sd_queue.lock);

  spin_unlock(&sd_queue.lock);
}

// Send the data transfer request to the hardware.
static void
sd_start_transfer(struct Buf *buf)
{
  uint32_t cmd;
  size_t nblocks;

  assert(spin_holding(&sd_queue.lock));
  assert(buf->queue_link.prev == &sd_queue.head);
  assert(buf->block_size % SD_BLOCKLEN == 0);

  nblocks = buf->block_size / SD_BLOCKLEN;

  if (buf->flags & BUF_DIRTY) {
    pl180_begin_transfer(&mmci, buf->block_size, 0);
    cmd = (nblocks > 1) ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_BLOCK;
  } else {
    pl180_begin_transfer(&mmci, buf->block_size, 1);
    cmd = (nblocks > 1) ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK;
  }

  pl180_send_cmd(&mmci, cmd, buf->block_no * buf->block_size, SD_RESPONSE_R1, NULL);
}

// Handle the SD card interrupt. Complete the current data transfer operation
// and wake up the corresponding task.
static void
sd_irq(void)
{
  struct ListLink *link;
  struct Buf *buf, *next_buf;

  spin_lock(&sd_queue.lock);

  // Grab the first buffer in the queue to find out whether a read or write
  // operation is happening
  link = sd_queue.head.next;
  list_remove(link);

  buf = LIST_CONTAINER(link, struct Buf, queue_link);

  assert((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID);
  assert(buf->block_size % SD_BLOCKLEN == 0);

  // Transfer the data and update the corresponding buffer flags.
  if (buf->flags & BUF_DIRTY) {
    pl180_send_data(&mmci, buf->data, buf->block_size);
    buf->flags &= ~BUF_DIRTY;
  } else {
    pl180_receive_data(&mmci, buf->data, buf->block_size);
    buf->flags |= BUF_VALID;
  }

  // Multiple block transfers must be stopped manually by issuing CMD12.
  if (buf->block_size > SD_BLOCKLEN)
    pl180_send_cmd(&mmci, CMD_STOP_TRANSMISSION, 0, SD_RESPONSE_R1B, NULL);

  // Begin processing the next buffer in the queue.
  if (!list_empty(&sd_queue.head)) {
    next_buf = LIST_CONTAINER(sd_queue.head.next, struct Buf, queue_link);
    sd_start_transfer(next_buf);
  }

  spin_unlock(&sd_queue.lock);

  // Resume the task waiting for the buf data.
  wchan_wakeup_all(&buf->wait_queue);
}
