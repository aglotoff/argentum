// See "ARM PrimeCell Multimedia Card Interface (PL180) Technical Reference
// Manual"

#include <argentum/drivers/sd.h>

#include "pl180.h"

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

// Initialize the MMCI
int
pl180_init(struct PL180 *pl180, void *base)
{
  pl180->base = (volatile uint32_t *) base;

  // Power on, 3.6 volts, rod control
  pl180->base[MMCI_POWER] = MMCI_POWER_ON | (0xF << 2) | MMCI_POWER_ROD;

  return 0;
}

// Enable receive and transmit interrupts
void
pl180_intr_enable(struct PL180 *pl180)
{
  pl180->base[MMCI_MASK0] = MMCI_TX_FIFO_EMPTY | MMCI_RX_DATA_AVLBL;
}

// Send a command to the card.
int
pl180_send_command(struct PL180 *pl180, uint32_t cmd, uint32_t arg,
                   int resp_type, uint32_t *resp)
{
  uint32_t cmd_type, status, check_flags;

  // Set the command type bits based on the expected response type
  cmd_type = 0;
  if (resp_type) {
    cmd_type |= MMCI_CMD_RESPONSE;
    if (resp_type == RESPONSE_R2)
      cmd_type |= MMCI_CMD_LONG_RESP;
  }

  pl180->base[MMCI_ARGUMENT] = arg;
  pl180->base[MMCI_COMMAND]  = MMCI_CMD_ENABLE | cmd_type | (cmd & 0x3F) ;

  // Status bits to check based on the response type
  check_flags = cmd_type & MMCI_CMD_RESPONSE
    ? MMCI_CMD_RESP_END | MMCI_CMD_TIME_OUT | MMCI_CMD_CRC_FAIL
    : MMCI_CMD_SENT | MMCI_CMD_TIME_OUT;

  // Wait until the command is sent
  while (!(status = pl180->base[MMCI_STATUS] & check_flags))
    ;

  // Get the command response, if present
  if ((status & MMCI_CMD_RESP_END) && (resp != NULL)) {
    if (cmd_type & MMCI_CMD_LONG_RESP) {
      resp[3] = pl180->base[MMCI_RESPONSE0];
      resp[2] = pl180->base[MMCI_RESPONSE1];
      resp[1] = pl180->base[MMCI_RESPONSE2];
      resp[0] = pl180->base[MMCI_RESPONSE3];
    } else {
      resp[0] = pl180->base[MMCI_RESPONSE0];
    }
  }

  // Clear the status flags
  pl180->base[MMCI_CLEAR] = check_flags;

  return status & (MMCI_CMD_TIME_OUT | MMCI_CMD_CRC_FAIL);
}

// Prepare MMCI for data transfer.
void
pl180_start(struct PL180 *pl180, uint32_t data_length, int is_read)
{
  uint32_t data_ctrl;

  data_ctrl = (SD_BLOCKLEN_LOG << 4) | MMCI_DATA_ENABLE;
  if (is_read)
    data_ctrl |= MMCI_DATA_DIRECTION;

  pl180->base[MMCI_DATA_TIMER]  = 0xFFFF;
  pl180->base[MMCI_DATA_LENGTH] = data_length;
  pl180->base[MMCI_DATA_CTRL]   = data_ctrl;
}


// Read data from the card.
int
pl180_read_data(struct PL180 *pl180, void *buf, size_t n)
{
  uint32_t status, err_flags, *dst;

  // Status bits that indicate a read error.
  err_flags = MMCI_DATA_CRC_FAIL | MMCI_DATA_TIME_OUT | MMCI_RX_OVERRUN;

  // Read data from the receive FIFO
  dst = (uint32_t *) buf;
  while (n > 0) {
    status = pl180->base[MMCI_STATUS];
    if (status & err_flags)
      break;
    if (!(status & MMCI_RX_DATA_AVLBL))
      continue;

    *dst++ = pl180->base[MMCI_FIFO];
    n -= sizeof(uint32_t);
  }

  // Make sure the data block is successfully received
  do {
    status = pl180->base[MMCI_STATUS];
  } while (!(status & (err_flags | MMCI_DATA_BLOCK_END)));

  // Clear status flags
  pl180->base[MMCI_CLEAR] = err_flags | MMCI_DATA_BLOCK_END | MMCI_RX_DATA_AVLBL;

  return status & err_flags;
}

// Write data to the card
int
pl180_write_data(struct PL180 *pl180, const void *buf, size_t n)
{
  uint32_t status, err_flags;
  const uint32_t *src;

  // Status bits that indicate a write error
  err_flags = MMCI_DATA_CRC_FAIL | MMCI_DATA_TIME_OUT;

  // Write data to the transmit FIFO
  src = (const uint32_t *) buf;
  while (n > 0) {
    status = pl180->base[MMCI_STATUS];
    if (status & err_flags)
      break;
    if (!(status & MMCI_TX_FIFO_HALF))
      continue;

    pl180->base[MMCI_FIFO] = *src++;
    n -= sizeof(uint32_t);
  }

  // Make sure the data block is successfully transferred
  do {
    status = pl180->base[MMCI_STATUS];
  } while (!(status & (err_flags | MMCI_DATA_BLOCK_END)));

  // Clear status flags
  pl180->base[MMCI_CLEAR] = err_flags | MMCI_DATA_BLOCK_END | MMCI_TX_FIFO_HALF;

  return status & err_flags;
}
