#include <kernel/core/assert.h>
#include <kernel/drivers/sd.h>
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

static void sd_irq_task(int, void *);
static void sd_start_transfer(struct SD *, struct BufRequest *);

int
sd_init(struct SD *sd, struct SDOps *ops, void *ctx, int irq)
{
  uint32_t resp[4], rca;

  // Put each card into Idle State
  ops->send_cmd(ctx, CMD_GO_IDLE_STATE, 0, 0, NULL);

  // Request the card to send its valid operation conditions
  do {
    ops->send_cmd(ctx, CMD_APP, 0, SD_RESPONSE_R1, NULL) ;
    ops->send_cmd(ctx, CM_SD_SEND_OP_COND, OCR_VDD_MASK, SD_RESPONSE_R3,
                   resp);
  } while (!(resp[0] & OCR_BUSY));

  // Get the unique card identification (CID) number
  ops->send_cmd(ctx, CMD_ALL_SEND_CID, 0, SD_RESPONSE_R2, NULL);

  // Ask the card to publish a new relative card address (RCA), which
  // will be used to address the card later
  ops->send_cmd(ctx, CMD_SEND_RELATIVE_ADDR, 0, SD_RESPONSE_R6, &rca);

  // Select the card and put it into the Transfer State
  ops->send_cmd(ctx, CMD_SELECT_CARD, rca & 0xFFFF0000, SD_RESPONSE_R1B, NULL);

  // Set the block length (512 bytes) for all I/O operations
  ops->send_cmd(ctx, CMD_SET_BLOCKLEN, SD_BLOCKLEN, SD_RESPONSE_R1, NULL);

  sd->ops = ops;
  sd->ctx = ctx;

  // Initialize the buffer queue
  k_list_init(&sd->queue);
  k_mutex_init(&sd->mutex, "sd_queue");

  // Enable interrupts
  sd->ops->irq_enable(sd->ctx);

  interrupt_attach_task(irq, sd_irq_task, sd);

  return 0;
}

void
sd_request(struct SD *sd, struct BufRequest *req)
{
  struct Buf *buf = req->buf;

  if (buf->block_size % SD_BLOCKLEN != 0)
    k_panic("block size must be a multiple of %u", SD_BLOCKLEN);

  k_mutex_lock(&sd->mutex);

  // Add buffer to the queue.
  k_list_add_back(&sd->queue, &req->queue_link);

  // If the buffer is at the front of the queue, immediately send it to the
  // hardware.
  if (sd->queue.next == &req->queue_link)
    sd_start_transfer(sd, req);

  buf_request_wait(req, &sd->mutex);

  k_mutex_unlock(&sd->mutex);
}

// Send the data transfer request to the hardware.
static void
sd_start_transfer(struct SD *sd, struct BufRequest *req)
{
  struct Buf *buf = req->buf;
  uint32_t cmd, arg;
  size_t nblocks;

  k_assert(k_mutex_holding(&sd->mutex));
  k_assert(req->queue_link.prev == &sd->queue);
  k_assert(buf->block_size % SD_BLOCKLEN == 0);

  nblocks = buf->block_size / SD_BLOCKLEN;

  if (req->type == BUF_REQUEST_WRITE) {
    sd->ops->begin_transfer(sd->ctx, buf->block_size, 0);
    cmd = (nblocks > 1) ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_BLOCK;
  } else {
    sd->ops->begin_transfer(sd->ctx, buf->block_size, 1);
    cmd = (nblocks > 1) ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK;
  }

  arg = buf->block_no * buf->block_size;

  if (sd->ops->send_cmd(sd->ctx, cmd, arg, SD_RESPONSE_R1, NULL) != 0)
    k_panic("error sending cmd %d, arg %d", cmd, arg);
}

// Handle the SD card interrupts. Complete the current data transfer operation
// and wake up the corresponding task.
static void
sd_irq_task(int irq, void *arg)
{
  struct SD *sd = (struct SD *) arg;
  struct KListLink *link;
  struct BufRequest *req, *next_req;

  k_mutex_lock(&sd->mutex);

  if (k_list_is_empty(&sd->queue))
    k_panic("queue is empty");

  // Grab the first buffer in the queue to find out whether a read or write
  // operation is happening
  link = sd->queue.next;
  k_list_remove(link);

  req = K_CONTAINER_OF(link, struct BufRequest, queue_link);

  k_assert(req->buf->block_size % SD_BLOCKLEN == 0);

  // Transfer the data and update the corresponding buffer flags.
  if (req->type == BUF_REQUEST_WRITE) {
    if (sd->ops->send_data(sd->ctx, req->buf->data, req->buf->block_size) != 0)
      k_panic("error writing block %d", req->buf->block_no);
  } else {
    if (sd->ops->receive_data(sd->ctx, req->buf->data, req->buf->block_size) != 0)
      k_panic("error reading block %d", req->buf->block_no);
  }

  // Multiple block transfers must be stopped manually by issuing CMD12.
  if (req->buf->block_size > SD_BLOCKLEN)
    sd->ops->send_cmd(sd->ctx, CMD_STOP_TRANSMISSION, 0, SD_RESPONSE_R1B, NULL);

  arch_interrupt_unmask(irq);

  // Begin processing the next buffer in the queue.
  if (!k_list_is_empty(&sd->queue)) {
    next_req = K_CONTAINER_OF(sd->queue.next, struct BufRequest, queue_link);
    sd_start_transfer(sd, next_req);
  }

  buf_request_wakeup(req);

  k_mutex_unlock(&sd->mutex);
}
