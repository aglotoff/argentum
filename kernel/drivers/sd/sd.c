#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/drivers/gic.h>
#include <argentum/drivers/sd.h>
#include <argentum/fs/buf.h>
#include <argentum/mm/memlayout.h>
#include <argentum/kthread.h>
#include <argentum/spin.h>
#include <argentum/irq.h>

#include "pl180.h"

/*
 * ----------------------------------------------------------------------------
 * SD Card Driver
 * ----------------------------------------------------------------------------
 *
 * The driver keeps the list of pending buffer requests in a queue, processing
 * them one at a time.
 * 
 * For details on SD card programming, see "SD Specifications. Part 1. Physical
 * Layer Simplified Specification. Version 1.10".
 *
 */

// Card bus commands
#define CMD_GO_IDLE_STATE         0
#define CMD_ALL_SEND_CID          2
#define CMD_SEND_RELATIVE_ADDR    3
#define CMD_SELECT_CARD           7
#define CMD_SEND_IF_COND          8
#define CMD_STOP_TRANSMISSION     12
#define CMD_SET_BLOCKLEN          16
#define CMD_READ_SINGLE_BLOCK     17
#define CMD_READ_MULTIPLE_BLOCK   18
#define CMD_WRITE_BLOCK           24
#define CMD_WRITE_MULTIPLE_BLOCK  25
#define CM_SD_SEND_OP_COND        41
#define CMD_APP                   55

// OCR Register fields
#define SD_VDD_MASK               0xff8000    // VDD Voltage Window bitmask
#define SD_OCR_CCS                (1 << 30)   // Card Capacity Status
#define SD_OCR_BUSY               (1 << 31)   // Card power up status bit

static struct PL180 mmci;

// The queue of pending buffer requests
static struct {
  struct ListLink head;
  struct SpinLock lock;
} sd_queue;

static int sd_irq(void);

/**
 * Initialize the SD card driver.
 */
void
sd_init(void)
{
  uint32_t resp[4], rca;

  // Power on
  pl180_init(&mmci, PA2KVA(PHYS_MMCI));

  // Put each card into Idle State
  pl180_send_command(&mmci, CMD_GO_IDLE_STATE, 0, 0, NULL);

  // Check whether the card supports the supplied voltage (2.7-3.6V)
  pl180_send_command(&mmci, CMD_SEND_IF_COND, 0x1AA, RESPONSE_R7, resp);
  if ((resp[0] & 0xFFF) != 0x1AA)
    panic("incompatible voltage range");

  // Request the card to send its valid operation conditions
  do {
    pl180_send_command(&mmci, CMD_APP, 0, RESPONSE_R1, NULL) ;
    pl180_send_command(&mmci, CM_SD_SEND_OP_COND, SD_OCR_CCS | SD_VDD_MASK,
                     RESPONSE_R3, resp);
  } while (!(resp[0] & SD_OCR_BUSY));

  // Get the unique card identification (CID) number
  pl180_send_command(&mmci, CMD_ALL_SEND_CID, 0, RESPONSE_R2, NULL);

  // Ask the card to publish a new relative card address (RCA), which
  // will be used to address to address the card later
  pl180_send_command(&mmci, CMD_SEND_RELATIVE_ADDR, 0, RESPONSE_R6, &rca);

  // Select the card
  pl180_send_command(&mmci, CMD_SELECT_CARD, rca & 0xFFFF0000, RESPONSE_R1B, NULL);

  // Set the block length (512 bytes) for I/O operations
  pl180_send_command(&mmci, CMD_SET_BLOCKLEN, SD_BLOCKLEN, RESPONSE_R1, NULL);

  // Enable interrupts
  pl180_intr_enable(&mmci);
  irq_attach(IRQ_MCIA, sd_irq, 0);

  // Initialize the buffer queue
  list_init(&sd_queue.head);
  spin_init(&sd_queue.lock, "sd_queue");
}

// Send buffer to the hardware
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
    pl180_start(&mmci, buf->block_size, 0);
    cmd = (nblocks > 1) ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_BLOCK;
  } else {
    pl180_start(&mmci, buf->block_size, 1);
    cmd = (nblocks > 1) ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK;
  }

  pl180_send_command(&mmci, cmd, buf->block_no * buf->block_size, RESPONSE_R1, NULL);
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
  if (!mutex_holding(&buf->mutex))
    panic("buf not locked");
  if ((buf->flags & (BUF_DIRTY | BUF_VALID)) == BUF_VALID)
    panic("nothing to do");
  if (buf->dev != 0)
    panic("dev must be 0");
  if (buf->block_size % SD_BLOCKLEN != 0)
    panic("block size must be a multiple of %u", SD_BLOCKLEN);

  spin_lock(&sd_queue.lock);

  // Add the request to the queue.
  list_add_back(&sd_queue.head, &buf->queue_link);

  // If the buffer is at the front of the queue, immediately send it to the
  // hardware.
  if (sd_queue.head.next == &buf->queue_link)
    sd_start_transfer(buf);

  // Wait for the R/W operation to finish
  while ((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID)
    kthread_sleep(&buf->wait_queue, &sd_queue.lock);

  spin_unlock(&sd_queue.lock);
}

/**
 * Handle the SD card interrupt. Complete the current data transfer operation
 * and wake up the corresponding process.
 */ 
static int
sd_irq(void)
{
  struct ListLink *link;
  struct Buf *buf, *next_buf;
  size_t nblocks;

  spin_lock(&sd_queue.lock);

  // Grab the first buffer in the queue to find out whether a read or write
  // operation is happening
  link = sd_queue.head.next;
  list_remove(link);

  buf  = LIST_CONTAINER(link, struct Buf, queue_link);

  assert(buf->block_size % SD_BLOCKLEN == 0);

  nblocks = buf->block_size / SD_BLOCKLEN;

  // Transfer the data and update the corresponding buffer flags.
  if (buf->flags & BUF_DIRTY) {
    pl180_write_data(&mmci, buf->data, buf->block_size);
    buf->flags &= ~BUF_DIRTY;
  } else {
    pl180_read_data(&mmci, buf->data, buf->block_size);
    buf->flags |= BUF_VALID;
  }

  // Multiple block transfers must be stopped manually by issuing CMD12
  if (nblocks > 1)
    pl180_send_command(&mmci, CMD_STOP_TRANSMISSION, 0, RESPONSE_R1B, NULL);

  // Begin processing the next waiting buffer
  if (!list_empty(&sd_queue.head)) {
    next_buf = LIST_CONTAINER(sd_queue.head.next, struct Buf, queue_link);
    sd_start_transfer(next_buf);
  }

  spin_unlock(&sd_queue.lock);

  kthread_wakeup(&buf->wait_queue);

  return 0;
}
