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
#include <argentum/trap.h>

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

// Response types
#define RESPONSE_R1               1
#define RESPONSE_R1B              2
#define RESPONSE_R2               3
#define RESPONSE_R3               4
#define RESPONSE_R6               7
#define RESPONSE_R7               8

// OCR Register fields
#define SD_VDD_MASK               0xff8000    // VDD Voltage Window bitmask
#define SD_OCR_CCS                (1 << 30)   // Card Capacity Status
#define SD_OCR_BUSY               (1 << 31)   // Card power up status bit

#define SD_BLOCKLEN               512         // Single block length in bytes
#define SD_BLOCKLEN_LOG           9           // log2 of SD_BLOCKLEN

static int  mmci_init(void);
static int  mmci_send_command(uint32_t, uint32_t, int, uint32_t *);
static int  mmci_read_data(void *, size_t);
static int  mmci_write_data(const void *, size_t);
static void mmci_start(uint32_t, int);
static void mmci_intr_enable(void);

// The queue of pending buffer requests
static struct {
  struct ListLink head;
  struct SpinLock lock;
} sd_queue;

/**
 * Initialize the SD card driver.
 */
void
sd_init(void)
{
  uint32_t resp[4], rca;

  // Power on
  mmci_init();

  // Put each card into Idle State
  mmci_send_command(CMD_GO_IDLE_STATE, 0, 0, NULL);

  // Check whether the card supports the supplied voltage (2.7-3.6V)
  mmci_send_command(CMD_SEND_IF_COND, 0x1AA, RESPONSE_R7, resp);
  if ((resp[0] & 0xFFF) != 0x1AA)
    panic("incompatible voltage range");

  // Request the card to send its valid operation conditions
  do {
    mmci_send_command(CMD_APP, 0, RESPONSE_R1, NULL) ;
    mmci_send_command(CM_SD_SEND_OP_COND, SD_OCR_CCS | SD_VDD_MASK,
                     RESPONSE_R3, resp);
  } while (!(resp[0] & SD_OCR_BUSY));

  // Get the unique card identification (CID) number
  mmci_send_command(CMD_ALL_SEND_CID, 0, RESPONSE_R2, NULL);

  // Ask the card to publish a new relative card address (RCA), which
  // will be used to address to address the card later
  mmci_send_command(CMD_SEND_RELATIVE_ADDR, 0, RESPONSE_R6, &rca);

  // Select the card
  mmci_send_command(CMD_SELECT_CARD, rca & 0xFFFF0000, RESPONSE_R1B, NULL);

  // Set the block length (512 bytes) for I/O operations
  mmci_send_command(CMD_SET_BLOCKLEN, SD_BLOCKLEN, RESPONSE_R1, NULL);

  // Enable interrupts
  mmci_intr_enable();
  gic_enable(IRQ_MCIA, 0);

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
    mmci_start(buf->block_size, 0);
    cmd = (nblocks > 1) ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_BLOCK;
  } else {
    mmci_start(buf->block_size, 1);
    cmd = (nblocks > 1) ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK;
  }

  mmci_send_command(cmd, buf->block_no * buf->block_size, RESPONSE_R1, NULL);
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
void
sd_intr(void)
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
    mmci_write_data(buf->data, buf->block_size);
    buf->flags &= ~BUF_DIRTY;
  } else {
    mmci_read_data(buf->data, buf->block_size);
    buf->flags |= BUF_VALID;
  }

  // Multiple block transfers must be stopped manually by issuing CMD12
  if (nblocks > 1)
    mmci_send_command(CMD_STOP_TRANSMISSION, 0, RESPONSE_R1B, NULL);

  // Begin processing the next waiting buffer
  if (!list_empty(&sd_queue.head)) {
    next_buf = LIST_CONTAINER(sd_queue.head.next, struct Buf, queue_link);
    sd_start_transfer(next_buf);
  }

  spin_unlock(&sd_queue.lock);

  kthread_wakeup(&buf->wait_queue);
}


/*
 * ----------------------------------------------------------------------------
 * MultiMedia Card Interface
 * ----------------------------------------------------------------------------
 * 
 * See "ARM PrimeCell Multimedia Card Interface (PL180) Technical Reference
 * Manual"
 *
 */

// MCI registers, divided by 4 for use as uint32_t[] indices
#define MMCI_POWER        (0x000 / 4)   // Power control
  #define MMCI_POWER_ON         (3 << 0)    // Power-on
  #define MMCI_POWER_ROD        (1 << 7)    // Rod control
#define MMCI_CLOCK        (0x004 / 4)   // Clock control
#define MMCI_ARGUMENT     (0x008 / 4)   // Argument
#define MMCI_COMMAND      (0x00C / 4)   // Command
  #define MMCI_CMD_RESPONSE     (1 << 6)    // Wait for a response
  #define MMCI_CMD_LONG_RESP    (1 << 7)    // Receives a 136-bit long response
  #define MMCI_CMD_INTERRUPT    (1 << 8)    // Wait for IRQ
  #define MMCI_CMD_PENDING      (1 << 9)    // Wait for CmdPend before sending
  #define MMCI_CMD_ENABLE       (1 << 10)   // CPSM is enabled
#define MMCI_RESP_CMD     (0x010 / 4)   // Response command
#define MMCI_RESPONSE0    (0x014 / 4)   // Response
#define MMCI_RESPONSE1    (0x018 / 4)   // Response
#define MMCI_RESPONSE2    (0x01C / 4)   // Response
#define MMCI_RESPONSE3    (0x020 / 4)   // Response
#define MMCI_DATA_TIMER   (0x024 / 4)   // Data timer
#define MMCI_DATA_LENGTH  (0x028 / 4)   // Data length
#define MMCI_DATA_CTRL    (0x02C / 4)   // Data control
  #define MMCI_DATA_ENABLE      (1 << 0)    // Data transafer enabled
  #define MMCI_DATA_DIRECTION   (1 << 1)    // From card to controller
#define MMCI_DATACNT      (0x030 / 4)   // Data counter
#define MMCI_STATUS       (0x034 / 4)   // Status
  #define MMCI_CMD_CRC_FAIL     (1 << 0)    // Command CRC check failed
  #define MMCI_DATA_CRC_FAIL    (1 << 1)    // Data CRC check failed
  #define MMCI_CMD_TIME_OUT     (1 << 2)    // Command response timeout
  #define MMCI_DATA_TIME_OUT    (1 << 3)    // Data timeout
  #define MMCI_TX_UNDERRUN      (1 << 4)    // Transmit FIFO underrun error
  #define MMCI_RX_OVERRUN       (1 << 5)    // Receive FIFO overrun error
  #define MMCI_CMD_RESP_END     (1 << 6)    // Command CRC check passed
  #define MMCI_CMD_SENT         (1 << 7)    // Command sent
  #define MMCI_DATA_END         (1 << 8)    // Data end
  #define MMCI_START_BIT_ERR    (1 << 9)    // Start bit not detected
  #define MMCI_DATA_BLOCK_END   (1 << 10)   // Data block sent/received
  #define MMCI_CMD_ACTIVE       (1 << 11)   // Command transfer in progress
  #define MMCI_TX_ACTIVE        (1 << 12)   // Data transmit in progress
  #define MMCI_RX_ACTIVE        (1 << 13)   // Data receive in progress
  #define MMCI_TX_FIFO_HALF     (1 << 14)   // Transmit FIFO half empty
  #define MMCI_RX_FIFO_HALF     (1 << 15)   // Receive FIFO half full
  #define MMCI_TX_FIFO_FULL     (1 << 16)   // Transmit FIFO full
  #define MMCI_RX_FIFO_FULL     (1 << 17)   // Receive FIFO full
  #define MMCI_TX_FIFO_EMPTY    (1 << 18)   // Transmit FIFO empty
  #define MMCI_RX_FIFO_EMPTY    (1 << 19)   // Receive FIFO empty
  #define MMCI_TX_DATA_AVLBL    (1 << 20)   // Transmit FIFO data available
  #define MMCI_RX_DATA_AVLBL    (1 << 21)   // Receive FIFO data available
#define MMCI_CLEAR        (0x038 / 4)   // Clear
#define MMCI_MASK0        (0x03C / 4)   // Interrupt 0 mask
#define MMCI_MASK1        (0x040 / 4)   // Interrupt 1 mask
#define MMCI_SELECT       (0x044 / 4)   // Secure digital memory card select
#define MMCI_FIFO_CNT     (0x048 / 4)   // FIFO counter
#define MMCI_FIFO         (0x080 / 4)   // Data FIFO
#define MMCI_PERIPH_ID0   (0xFE0 / 4)   // Peripheral identification bits 7:0
#define MMCI_PERIPH_ID1   (0xFE4 / 4)   // Peripheral identification bits 15:8
#define MMCI_PERIPH_ID2   (0xFE8 / 4)   // Peripheral identification bits 23:16
#define MMCI_PERIPH_ID3   (0xFEC / 4)   // Peripheral identification bits 31:24
#define MMCI_PCELL_ID0    (0xFF0 / 4)   // PrimeCell identification bits 7:0
#define MMCI_PCELL_ID1    (0xFF4 / 4)   // PrimeCell identification bits 15:8
#define MMCI_PCELL_ID2    (0xFF8 / 4)   // PrimeCell identification bits 23:16
#define MMCI_PCELL_ID3    (0xFFC / 4)   // PrimeCell identification bits 31:24

static volatile uint32_t *mmci;

// Enable receive and transmit interrupts
static void
mmci_intr_enable(void)
{
  mmci[MMCI_MASK0] = MMCI_TX_FIFO_EMPTY | MMCI_RX_DATA_AVLBL;
}

// Initialize the MMCI
static int
mmci_init(void)
{
  mmci = (volatile uint32_t *) PA2KVA(PHYS_MMCI);

  // Power on, 3.6 volts, rod control
  mmci[MMCI_POWER] = MMCI_POWER_ON | (0xF << 2) | MMCI_POWER_ROD;

  return 0;
}

// Send a command to the card.
static int
mmci_send_command(uint32_t cmd, uint32_t arg, int resp_type, uint32_t *resp)
{
  uint32_t cmd_type, status, check_flags;

  // Set the command type bits based on the expected response type
  cmd_type = 0;
  if (resp_type) {
    cmd_type |= MMCI_CMD_RESPONSE;
    if (resp_type == RESPONSE_R2)
      cmd_type |= MMCI_CMD_LONG_RESP;
  }

  mmci[MMCI_ARGUMENT] = arg;
  mmci[MMCI_COMMAND]  = MMCI_CMD_ENABLE | cmd_type | (cmd & 0x3F) ;

  // Status bits to check based on the response type
  check_flags = cmd_type & MMCI_CMD_RESPONSE
    ? MMCI_CMD_RESP_END | MMCI_CMD_TIME_OUT | MMCI_CMD_CRC_FAIL
    : MMCI_CMD_SENT | MMCI_CMD_TIME_OUT;

  // Wait until the command is sent
  while (!(status = mmci[MMCI_STATUS] & check_flags))
    ;

  // Get the command response, if present
  if ((status & MMCI_CMD_RESP_END) && (resp != NULL)) {
    if (cmd_type & MMCI_CMD_LONG_RESP) {
      resp[3] = mmci[MMCI_RESPONSE0];
      resp[2] = mmci[MMCI_RESPONSE1];
      resp[1] = mmci[MMCI_RESPONSE2];
      resp[0] = mmci[MMCI_RESPONSE3];
    } else {
      resp[0] = mmci[MMCI_RESPONSE0];
    }
  }

  // Clear the status flags
  mmci[MMCI_CLEAR] = check_flags;

  return status & (MMCI_CMD_TIME_OUT | MMCI_CMD_CRC_FAIL);
}

// Prepare MMCI for data transfer.
static void
mmci_start(uint32_t data_length, int is_read)
{
  uint32_t data_ctrl;

  data_ctrl = (SD_BLOCKLEN_LOG << 4) | MMCI_DATA_ENABLE;
  if (is_read)
    data_ctrl |= MMCI_DATA_DIRECTION;

  mmci[MMCI_DATA_TIMER]  = 0xFFFF;
  mmci[MMCI_DATA_LENGTH] = data_length;
  mmci[MMCI_DATA_CTRL]   = data_ctrl;
}

// Read data from the card.
static int
mmci_read_data(void *buf, size_t n)
{
  uint32_t status, err_flags, *dst;

  // Status bits that indicate a read error.
  err_flags = MMCI_DATA_CRC_FAIL | MMCI_DATA_TIME_OUT | MMCI_RX_OVERRUN;

  // Read data from the receive FIFO
  dst = (uint32_t *) buf;
  while (n > 0) {
    status = mmci[MMCI_STATUS];
    if (status & err_flags)
      break;
    if (!(status & MMCI_RX_DATA_AVLBL))
      continue;

    *dst++ = mmci[MMCI_FIFO];
    n -= sizeof(uint32_t);
  }

  // Make sure the data block is successfully received
  do {
    status = mmci[MMCI_STATUS];
  } while (!(status & (err_flags | MMCI_DATA_BLOCK_END)));

  // Clear status flags
  mmci[MMCI_CLEAR] = err_flags | MMCI_DATA_BLOCK_END | MMCI_RX_DATA_AVLBL;

  return status & err_flags;
}

// Write data to the card
static int
mmci_write_data(const void *buf, size_t n)
{
  uint32_t status, err_flags;
  const uint32_t *src;

  // Status bits that indicate a write error
  err_flags = MMCI_DATA_CRC_FAIL | MMCI_DATA_TIME_OUT;

  // Write data to the transmit FIFO
  src = (const uint32_t *) buf;
  while (n > 0) {
    status = mmci[MMCI_STATUS];
    if (status & err_flags)
      break;
    if (!(status & MMCI_TX_FIFO_HALF))
      continue;

    mmci[MMCI_FIFO] = *src++;
    n -= sizeof(uint32_t);
  }

  // Make sure the data block is successfully transferred
  do {
    status = mmci[MMCI_STATUS];
  } while (!(status & (err_flags | MMCI_DATA_BLOCK_END)));

  // Clear status flags
  mmci[MMCI_CLEAR] = err_flags | MMCI_DATA_BLOCK_END | MMCI_TX_FIFO_HALF;

  return status & err_flags;
}
