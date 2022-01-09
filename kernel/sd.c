#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "buf.h"
#include "console.h"
#include "gic.h"
#include "process.h"
#include "sd.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "trap.h"
#include "vm.h"

static int  mci_init(void);
static int  mci_send_command(uint32_t, uint32_t, int, uint32_t *);
static void mci_request(struct Buf *);
static int  mci_read_data(void *, size_t);
static int  mci_write_data(const void *, size_t);

static struct ListLink sd_queue;
static struct Spinlock sd_lock;

int
sd_init(void)
{
  mci_init();

  list_init(&sd_queue);
  spin_init(&sd_lock, "sd");

  return 0;
}

void
sd_intr(void)
{
  struct ListLink *l;
  struct Buf *buf;

  spin_lock(&sd_lock);

  l = sd_queue.next;
  buf = LIST_CONTAINER(l, struct Buf, queue_link);

  if (buf->flags & BUF_DIRTY)
    mci_write_data(buf->data, BLOCK_SIZE);
  else
    mci_read_data(buf->data, BLOCK_SIZE);

  if (l->next != &sd_queue)
    mci_request(LIST_CONTAINER(l->next, struct Buf, queue_link));

  list_remove(l);
  buf->flags = BUF_VALID;

  spin_unlock(&sd_lock);

  process_wakeup(&buf->wait_queue);
}

void
sd_request(struct Buf *buf)
{
  if (!sleep_holding(&buf->lock))
    panic("buf not locked");

  if ((buf->flags & (BUF_DIRTY | BUF_VALID)) == BUF_VALID) {
    warn("nothing to do");
    return;
  }

  spin_lock(&sd_lock);

  list_add_back(&sd_queue, &buf->queue_link);

  if (sd_queue.next == &buf->queue_link)
    mci_request(buf);

  while ((buf->flags & (BUF_DIRTY | BUF_VALID)) != BUF_VALID)
    process_sleep(&buf->wait_queue, &sd_lock);

  spin_unlock(&sd_lock);
}


/*
 * ----------------------------------------------------------------------------
 * 
 * ----------------------------------------------------------------------------
 * 
 * See "SD Specifications. Part 1. Physical Layer Simplified Specification.
 * Version 8.00" and "ARM PrimeCell Multimedia Card Interface (PL180) Technical
 * Reference Manual"
 */

// MCI base address
#define MCI_BASE        0x10005000

// MCI registers, shifted right by 2 bits for use as uint32_t[] indices
#define MCI_POWER       (0x000 / 4)   // Power control
  #define MCI_PWR_OFF         (0 << 0)    // Power-off
  #define MCI_PWR_UP          (2 << 0)    // Power-up
  #define MCI_PWR_ON          (3 << 0)    // Power-on
  #define MCI_PWR_OPEN_DRAIN  (1 << 6)    // MCICMD output control
  #define MCI_PWR_ROD         (1 << 7)    // Rod control
#define MCI_CLOCK       (0x004 / 4)   // Clock control
#define MCI_ARGUMENT    (0x008 / 4)   // Argument
#define MCI_COMMAND     (0x00C / 4)   // Command
  #define MCI_CMD_RESPONSE    (1 << 6)    // Wait for a response
  #define MCI_CMD_LONG_RESP   (1 << 7)    // Receives a 136-bit long response
  #define MCI_CMD_INTERRUPT   (1 << 8)    // Wait for IRQ
  #define MCI_CMD_PENDING     (1 << 9)    // Wait for CmdPend before sending
  #define MCI_CMD_ENABLE      (1 << 10)   // CPSM is enabled
#define MCI_RESPCMD     (0x010 / 4)   // Response command
#define MCI_RESPONSE0   (0x014 / 4)   // Response
#define MCI_RESPONSE1   (0x018 / 4)   // Response
#define MCI_RESPONSE2   (0x01C / 4)   // Response
#define MCI_RESPONSE3   (0x020 / 4)   // Response
#define MCI_DATATIMER   (0x024 / 4)   // Data timer
#define MCI_DATALENGTH  (0x028 / 4)   // Data length
#define MCI_DATACTRL    (0x02C / 4)   // Data control
  #define MCI_DATACTRL_EN     (1 << 0)    // Data transafer enabled
  #define MCI_DATACTRL_DIR    (1 << 1)    // From card to controller
  #define MCI_DATACTRL_MODE   (1 << 2)    // Stream data transfer
  #define MCI_DATACTRL_DMA_EN (1 << 3)    // DMA enabled
#define MCI_DATACNT     (0x030 / 4)   // Data counter
#define MCI_STATUS      (0x034 / 4)   // Status
  #define MCI_CMD_CRC_FAIL    (1 << 0)    // Command CRC check failed
  #define MCI_DATA_CRC_FAIL   (1 << 1)    // Data CRC check failed
  #define MCI_CMD_TIME_OUT    (1 << 2)    // Command response timeout
  #define MCI_DATA_TIME_OUT   (1 << 3)    // Data timeout
  #define MCI_TX_UNDERRUN     (1 << 4)    // Transmit FIFO underrun error
  #define MCI_RX_OVERRUN      (1 << 5)    // Receive FIFO overrun error
  #define MCI_CMD_RESP_END    (1 << 6)    // Command CRC check passed
  #define MCI_CMD_SENT        (1 << 7)    // Command sent
  #define MCI_DATA_END        (1 << 8)    // Data end
  #define MCI_START_BIT_ERR   (1 << 9)    // Start bit not detected
  #define MCI_DATA_BLOCK_END  (1 << 10)   // Data block sent/received
  #define MCI_CMD_ACTIVE      (1 << 11)   // Command transfer in progress
  #define MCI_TX_ACTIVE       (1 << 12)   // Data transmit in progress
  #define MCI_RX_ACTIVE       (1 << 13)   // Data receive in progress
  #define MCI_TX_FIFO_HALF    (1 << 14)   // Transmit FIFO half empty
  #define MCI_RX_FIFO_HALF    (1 << 15)   // Receive FIFO half full
  #define MCI_TX_FIFO_FULL    (1 << 16)   // Transmit FIFO full
  #define MCI_RX_FIFO_FULL    (1 << 17)   // Receive FIFO full
  #define MCI_TX_FIFO_EMPTY   (1 << 18)   // Transmit FIFO empty
  #define MCI_RX_FIFO_EMPTY   (1 << 19)   // Receive FIFO empty
  #define MCI_TX_DATA_AVLBL   (1 << 20)   // Transmit FIFO data available
  #define MCI_RX_DATA_AVLBL   (1 << 21)   // Receive FIFO data available
#define MCI_CLEAR       (0x038 / 4)   // Clear
#define MCI_MASK0       (0x03C / 4)   // Interrupt 0 mask
#define MCI_MASK1       (0x040 / 4)   // Interrupt 1 mask
#define MCI_SELECT      (0x044 / 4)   // Secure digital memory card select
#define MCI_FIFOCNT     (0x048 / 4)   // FIFO counter
#define MCI_FIFO        (0x080 / 4)   // Data FIFO
#define MCI_PERIPHID0   (0xFE0 / 4)   // Peripheral identification, bits 7:0
#define MCI_PERIPHID1   (0xFE4 / 4)   // Peripheral identification, bits 15:8
#define MCI_PERIPHID2   (0xFE8 / 4)   // Peripheral identification, bits 23:16
#define MCI_PERIPHID3   (0xFEC / 4)   // Peripheral identification, bits 31:24
#define MCI_PCELLID0    (0xFF0 / 4)   // PrimeCell identification, bits 7:0
#define MCI_PCELLID1    (0xFF4 / 4)   // PrimeCell identification, bits 15:8
#define MCI_PCELLID2    (0xFF8 / 4)   // PrimeCell identification, bits 23:16
#define MCI_PCELLID3    (0xFFC / 4)   // PrimeCell identification, bits 31:24

// SD Memory Card bus commands.
// See Section 4.7.4 of Physical Layer Simplified Specification
#define SD_GO_IDLE_STATE          0
#define SD_ALL_SEND_CID           2
#define SD_SEND_RELATIVE_ADDR     3
#define SD_SELECT_CARD            7
#define SD_SEND_IF_COND           8
#define SD_SET_BLOCKLEN           16
#define SD_READ_SINGLE_BLOCK      17
#define SD_READ_MULTIPLE_BLOCK    18
#define SD_SET_BLOCK_COUNT        23
#define SD_WRITE_BLOCK            24
#define SD_WRITE_MULTIPLE_BLOCK   25
#define SD_SD_SEND_OP_COND        41
#define SD_APP_CMD                55

// SD Memory Card response types.
// See Section 4.9 of Physical Layer Simplified Specification
#define SD_RESPONSE_R1            1
#define SD_RESPONSE_R1B           2
#define SD_RESPONSE_R2            3
#define SD_RESPONSE_R3            4
#define SD_RESPONSE_R6            7
#define SD_RESPONSE_R7            8

// OCR Register fields.
// See Section 5.1 of Physical Layer Simplified Specification
#define SD_VDD_MASK               0xff8000    // VDD Voltage Window bitmask
#define SD_OCR_CCS                (1 << 30)   // Card Capacity Status
#define SD_OCR_BUSY               (1 << 31)   // Card power up status bit

static volatile uint32_t *mci;

static int
mci_init(void)
{
  uint32_t resp[4];
  int attempts;
  
  mci = (volatile uint32_t *) vm_map_mmio(MCI_BASE, 4096);

  // Power on, 3.6 volts, rod control
  mci[MCI_POWER] = MCI_PWR_ON | (0xF << 2) | MCI_PWR_ROD;

  // Reset all cards to Idle State
  mci_send_command(SD_GO_IDLE_STATE, 0, 0, NULL);

  // Check whether the card supports the supplied voltage (2.7-3.6V)
  mci_send_command(SD_SEND_IF_COND, 0x1AA, SD_RESPONSE_R7, resp);
  if ((resp[0] & 0xFF) != 0xAA)
    return -ENODEV;

  // Attempt to initialize card by repeatedly issuing ACMD41 until the busy
  // bit in the OCR is set to 1.
  attempts = 0;
  resp[0] = 0;
  do {
    if (++attempts > 100)
      return -ENODEV;

    mci_send_command(SD_APP_CMD, 0, SD_RESPONSE_R1, NULL) ;
    mci_send_command(SD_SD_SEND_OP_COND, SD_OCR_CCS | SD_VDD_MASK,
                    SD_RESPONSE_R3, resp);

    // TODO: 10ms delay
  } while (!(resp[0] & SD_OCR_BUSY));

  // Get the unique card identifiaction number
  mci_send_command(SD_ALL_SEND_CID, 0, SD_RESPONSE_R2, NULL);

  // Ask the card to publish a new relative card address (RCA)
  mci_send_command(SD_SEND_RELATIVE_ADDR, 0, SD_RESPONSE_R6, resp);

  // Select the card and put it into the Transfer state
  mci_send_command(SD_SELECT_CARD, resp[0], SD_RESPONSE_R1B, NULL);

  // Set the block length (512 bytes) for I/O operations.
  mci_send_command(SD_SET_BLOCKLEN, 512, SD_RESPONSE_R1, NULL);

  // Enable interrupts.
  mci[MCI_MASK0] = MCI_TX_FIFO_EMPTY | MCI_RX_DATA_AVLBL;
  gic_enable(IRQ_MCIA, 0);

  return 0;
}

static int
mci_send_command(uint32_t cmd, uint32_t arg, int resp_type, uint32_t *resp)
{
  uint32_t cmd_flags, status, check_bits;

  if (mci[MCI_COMMAND] & MCI_CMD_ENABLE)
    mci[MCI_COMMAND] = 0;
  
  mci[MCI_ARGUMENT] = arg;

  cmd_flags = 0;
  if (resp_type) {
    cmd_flags |= MCI_CMD_RESPONSE;
    if (resp_type == SD_RESPONSE_R2)
      cmd_flags |= MCI_CMD_LONG_RESP;
  }

  mci[MCI_COMMAND] = MCI_CMD_ENABLE | cmd_flags | (cmd & 0x3F) ;

  check_bits = cmd_flags & MCI_CMD_RESPONSE
    ? MCI_CMD_RESP_END | MCI_CMD_TIME_OUT | MCI_CMD_CRC_FAIL
    : MCI_CMD_SENT | MCI_CMD_TIME_OUT;

  while (!((status = mci[MCI_STATUS]) & check_bits))
    ;

  if ((cmd_flags & MCI_CMD_RESPONSE) && (resp != NULL)) {
    if (cmd_flags & MCI_CMD_LONG_RESP) {
      resp[3] = mci[MCI_RESPONSE0];
      resp[2] = mci[MCI_RESPONSE1];
      resp[1] = mci[MCI_RESPONSE2];
      resp[0] = mci[MCI_RESPONSE3];
    } else {
      resp[0] = mci[MCI_RESPONSE0];
    }
  }

  mci[MCI_CLEAR] = check_bits;

  return status & (MCI_CMD_TIME_OUT | MCI_CMD_CRC_FAIL);
}

static void
mci_request(struct Buf *buf)
{
  unsigned cnt = BLOCK_SIZE / 512;
  
  mci[MCI_DATATIMER]  = 0xFFFF;
  mci[MCI_DATALENGTH] = BLOCK_SIZE;
  
  if (cnt > 1)
    mci_send_command(SD_SET_BLOCK_COUNT, cnt, SD_RESPONSE_R1, NULL);

  if (buf->flags & BUF_DIRTY) {
    // Data transfer enable, from controller to card, block size = 2^9
    mci[MCI_DATACTRL] = (9 << 4) | MCI_DATACTRL_EN;

    mci_send_command(cnt > 1 ? SD_WRITE_MULTIPLE_BLOCK : SD_WRITE_BLOCK,
                    buf->block_no * BLOCK_SIZE, SD_RESPONSE_R1, NULL);
  } else {
    // Data transfer enable, from card to controller, block size = 2^9
    mci[MCI_DATACTRL] = (9 << 4) | MCI_DATACTRL_DIR | MCI_DATACTRL_EN;

    mci_send_command(cnt > 1 ? SD_READ_MULTIPLE_BLOCK : SD_READ_SINGLE_BLOCK,
                    buf->block_no * BLOCK_SIZE, SD_RESPONSE_R1, NULL);
  }
}

static int
mci_read_data(void *buf, size_t n)
{
  uint32_t status, check_bits, *p;
  
  assert((n % sizeof(uint32_t)) == 0);

  p = (uint32_t *) buf;
  check_bits = MCI_DATA_CRC_FAIL | MCI_DATA_TIME_OUT | MCI_RX_OVERRUN;

  status = mci[MCI_STATUS];
  while (!(status & check_bits) && (n != 0)) {
    if (status & MCI_RX_DATA_AVLBL) {
      *p++ = mci[MCI_FIFO];
      n -= sizeof(uint32_t);
    }

    status = mci[MCI_STATUS];
  }

  while (!(status & check_bits) && !(status & MCI_DATA_BLOCK_END))
    status = mci[MCI_STATUS];

  mci[MCI_CLEAR] = 0xFFFFFFFF;

  return status & check_bits;
}

static int
mci_write_data(const void *buf, size_t n)
{
  uint32_t status, check_bits;
  const uint32_t *p;
  
  assert((n % sizeof(uint32_t)) == 0);

  p = (const uint32_t *) buf;
  check_bits = MCI_DATA_CRC_FAIL | MCI_DATA_TIME_OUT;

  status = mci[MCI_STATUS];
  while (!(status & check_bits) && (n != 0)) {
    if (status & MCI_TX_FIFO_HALF) {
      mci[MCI_FIFO] = *p++;
      n -= sizeof(uint32_t);
    }

    status = mci[MCI_STATUS];
  }

  while (!(status & check_bits) && !(status & MCI_DATA_BLOCK_END))
    status = mci[MCI_STATUS];

  mci[MCI_CLEAR] = 0xFFFFFFFF;

  return status & check_bits;
}
