#include <kernel/drivers/sd.h>
#include <kernel/drivers/pl180.h>

/*******************************************************************************
 * ARM PrimeCell Multimedia Card Interface (PL180) driver.
 * 
 * Note: this code works in QEMU but hasn't been tested on real hardware!
 * 
 * See ARM PrimeCell Multimedia Card Interface (PL180) Technical Reference
 * Manual.
 ******************************************************************************/

// MCI registers, divided by 4 for use as uint32_t[] indices
enum {
  MCI_POWER       = (0x000 / 4),  // Power control register
  MCI_CLOCK       = (0x004 / 4),  // Clock control register
  MCI_ARGUMENT    = (0x008 / 4),  // Argument register
  MCI_COMMAND     = (0x00C / 4),  // Command register
  MCI_RESP_CMD    = (0x010 / 4),  // Response command register
  MCI_RESPONSE0   = (0x014 / 4),  // Response register 0
  MCI_RESPONSE1   = (0x018 / 4),  // Response register 1
  MCI_RESPONSE2   = (0x01C / 4),  // Response register 2
  MCI_RESPONSE3   = (0x020 / 4),  // Response register 3
  MCI_DATA_TIMER  = (0x024 / 4),  // Data timer register
  MCI_DATA_LENGTH = (0x028 / 4),  // Data length register
  MCI_DATA_CTRL   = (0x02C / 4),  // Data control register
  MCI_DATA_CNT    = (0x030 / 4),  // Data counter register
  MCI_STATUS      = (0x034 / 4),  // Status register
  MCI_CLEAR       = (0x038 / 4),  // Clear register
  MCI_MASK0       = (0x03C / 4),  // Interrupt mask register 0
  MCI_MASK1       = (0x040 / 4),  // Interrupt mask register 1
  MCI_SELECT      = (0x044 / 4),  // Secure digital memory card select register
  MCI_FIFO_CNT    = (0x048 / 4),  // FIFO counter register
  MCI_FIFO        = (0x080 / 4),  // Data FIFO register
  MCI_PERIPH_ID0  = (0xFE0 / 4),  // Peripheral identification register 0
  MCI_PERIPH_ID1  = (0xFE4 / 4),  // Peripheral identification register 1
  MCI_PERIPH_ID2  = (0xFE8 / 4),  // Peripheral identification register 2
  MCI_PERIPH_ID3  = (0xFEC / 4),  // Peripheral identification register 3
  MCI_PCELL_ID0   = (0xFF0 / 4),  // PrimeCell identification register 0
  MCI_PCELL_ID1   = (0xFF4 / 4),  // PrimeCell identification register 1
  MCI_PCELL_ID2   = (0xFF8 / 4),  // PrimeCell identification register 2
  MCI_PCELL_ID3   = (0xFFC / 4),  // PrimeCell identification register 3
};

// Power control register bits
enum {
  MCI_POWER_CTRL_ON = (3 << 0),   // Power-on
  MCI_POWER_ROD     = (1 << 7),   // Rod control
};

// Command register bits
enum {
  MCI_COMMAND_RESPONSE  = (1 << 6),   // Wait for a response
  MCI_COMMAND_LONG_RESP = (1 << 7),   // Receives a 136-bit long response
  MCI_COMMAND_ENABLE    = (1 << 10),  // CPSM is enabled
};

// Data control register bits
enum {
  MCI_DATA_CTRL_ENABLE    = (1 << 0),   // Data transafer enabled
  MCI_DATA_CTRL_DIRECTION = (1 << 1),   // From card to controller
};

// Status flags
enum {
  MCI_CMD_CRC_FAIL   = (1 << 0),    // Command CRC check failed
  MCI_DATA_CRC_FAIL  = (1 << 1),    // Data CRC check failed
  MCI_CMD_TIME_OUT   = (1 << 2),    // Command response timeout
  MCI_DATA_TIME_OUT  = (1 << 3),    // Data timeout
  MCI_TX_UNDERRUN    = (1 << 4),    // Transmit FIFO underrun error
  MCI_RX_OVERRUN     = (1 << 5),    // Receive FIFO overrun error
  MCI_CMD_RESP_END   = (1 << 6),    // Command CRC check passed
  MCI_CMD_SENT       = (1 << 7),    // Command sent
  MCI_DATA_END       = (1 << 8),    // Data end
  MCI_START_BIT_ERR  = (1 << 9),    // Start bit not detected
  MCI_DATA_BLOCK_END = (1 << 10),   // Data block sent/received
  MCI_CMD_ACTIVE     = (1 << 11),   // Command transfer in progress
  MCI_TX_ACTIVE      = (1 << 12),   // Data transmit in progress
  MCI_RX_ACTIVE      = (1 << 13),   // Data receive in progress
  MCI_TX_FIFO_HALF   = (1 << 14),   // Transmit FIFO half empty
  MCI_RX_FIFO_HALF   = (1 << 15),   // Receive FIFO half full
  MCI_TX_FIFO_FULL   = (1 << 16),   // Transmit FIFO full
  MCI_RX_FIFO_FULL   = (1 << 17),   // Receive FIFO full
  MCI_TX_FIFO_EMPTY  = (1 << 18),   // Transmit FIFO empty
  MCI_RX_FIFO_EMPTY  = (1 << 19),   // Receive FIFO empty
  MCI_TX_DATA_AVLBL  = (1 << 20),   // Transmit FIFO data available
  MCI_RX_DATA_AVLBL  = (1 << 21),   // Receive FIFO data available
};

/**
 * Initialize the MMCI driver.
 *
 * @param pl180 Pointer to the driver instance.
 * @param base Memory base address.
 *
 * @return 0 on success, a non-zero value on error. 
 */
int
pl180_init(struct PL180 *pl180, void *base)
{ 
  pl180->base = (volatile uint32_t *) base;

  // Power on, 3.6 volts, rod control.
  pl180->base[MCI_POWER] = MCI_POWER_CTRL_ON | (0xF << 2) | MCI_POWER_ROD;

  return 0;
}

/**
 * Enable interrupts on the card.
 * 
 * @param pl180 Pointer to the driver instance.
 */
void
pl180_k_irq_enable(struct PL180 *pl180)
{
  pl180->base[MCI_MASK0] = MCI_TX_FIFO_EMPTY | MCI_RX_DATA_AVLBL;
}

/**
 * Send command to the card.
 *
 * @param pl180 Pointer to the driver instance.
 * @param cmd The command index.
 * @param arg The command argument.
 * @param resp_type Response type.
 * @param resp Pointer to the block of memory to store the response.
 * 
 * @return 0 on success, a non-zero value on error.
 */
int
pl180_send_cmd(struct PL180 *pl180, uint32_t cmd, uint32_t arg, int resp_type,
               uint32_t *resp)
{
  uint32_t cmd_type, status, err_flags, flags;

  // Determine the command type bits based on the response type.
  cmd_type = MCI_COMMAND_ENABLE;
  if (resp_type) {
    cmd_type |= MCI_COMMAND_RESPONSE;
    if (resp_type == SD_RESPONSE_R2)
      cmd_type |= MCI_COMMAND_LONG_RESP;
  }

  // Send the command message.
  pl180->base[MCI_ARGUMENT] = arg;
  pl180->base[MCI_COMMAND]  = cmd_type | (cmd & 0x3F) ;

  // Static flags to be checked.
  err_flags = cmd_type & MCI_COMMAND_RESPONSE
            ? MCI_CMD_CRC_FAIL | MCI_CMD_TIME_OUT
            : MCI_CMD_TIME_OUT;
  flags = cmd_type & MCI_COMMAND_RESPONSE 
        ? err_flags | MCI_CMD_RESP_END
        : err_flags | MCI_CMD_SENT;

  do {
    status = pl180->base[MCI_STATUS];
  } while (!(status & flags));

  // Receive response, if present.
  if ((status & MCI_CMD_RESP_END) && (resp != NULL)) {
    if (cmd_type & MCI_COMMAND_LONG_RESP) {
      resp[3] = pl180->base[MCI_RESPONSE0];
      resp[2] = pl180->base[MCI_RESPONSE1];
      resp[1] = pl180->base[MCI_RESPONSE2];
      resp[0] = pl180->base[MCI_RESPONSE3];
    } else {
      resp[0] = pl180->base[MCI_RESPONSE0];
    }
  }

  // Clear the status flags.
  pl180->base[MCI_CLEAR] = status & flags;

  return status & err_flags;
}

/**
 * Prepare data transfer to or from the card.
 * 
 * @param pl180 Pointer to the driver instance.
 * @param data_length The number of bytes to be transferred.
 * @param direction Transfer direction: 0 = send, 1 = receive.
 */
void
pl180_begin_transfer(struct PL180 *pl180, uint32_t data_length, int direction)
{
  uint32_t data_ctrl;

  data_ctrl = (SD_BLOCKLEN_LOG << 4) | MCI_DATA_CTRL_ENABLE;
  if (direction)
    data_ctrl |= MCI_DATA_CTRL_DIRECTION;

  pl180->base[MCI_DATA_TIMER]  = 0xFFFF;
  pl180->base[MCI_DATA_LENGTH] = data_length;
  pl180->base[MCI_DATA_CTRL]   = data_ctrl;
}

/**
 * Receive data from the card.
 * 
 * @param pl180 Pointer to the driver instance.
 * @param buf Pointer to the memory block to store the data.
 * @param n The number of 32-bit words to read.
 * 
 * @return 0 on success, a non-zero value on error.
 */
int
pl180_receive_data(struct PL180 *pl180, void *buf, size_t n)
{
  uint32_t status, err_flags, flags, *dst;

  // Static flags to be checked.
  err_flags = MCI_DATA_CRC_FAIL | MCI_DATA_TIME_OUT | MCI_RX_OVERRUN
            | MCI_START_BIT_ERR;
  flags = err_flags | MCI_DATA_BLOCK_END;

  // Transfer data from the card.
  dst = (uint32_t *) buf;
  while (n > 0) {
    status = pl180->base[MCI_STATUS];

    if (status & err_flags)
      break;

    if (!(status & MCI_RX_DATA_AVLBL))
      continue;

    *dst++ = pl180->base[MCI_FIFO];
    n -= sizeof(uint32_t);
  }

  // Make sure the data block is completely received.
  do {
    status = pl180->base[MCI_STATUS];
  } while (!(status & flags));

  // Clear status flags.
  pl180->base[MCI_CLEAR] = status & flags;

  return status & err_flags;
}

/**
 * Send data to the card.
 * 
 * @param pl180 Pointer to the driver instance.
 * @param buf Pointer to the memory block to read the data from.
 * @param n The number of 32-bit words to write.
 * 
 * @return 0 on success, a non-zero value on error.
 */
int
pl180_send_data(struct PL180 *pl180, const void *buf, size_t n)
{
  uint32_t status, err_flags, flags;
  const uint32_t *src;

  // Static flags to be checked.
  err_flags = MCI_DATA_CRC_FAIL | MCI_DATA_TIME_OUT | MCI_TX_UNDERRUN
            | MCI_START_BIT_ERR;
  flags = err_flags | MCI_DATA_BLOCK_END;

  // Transfer data to the card.
  src = (const uint32_t *) buf;
  while (n > 0) {
    status = pl180->base[MCI_STATUS];

    if (status & err_flags)
      break;

    if (status & MCI_TX_FIFO_FULL)
      continue;

    pl180->base[MCI_FIFO] = *src++;
    n -= sizeof(uint32_t);
  }

  // Make sure the data block is completely transferred.
  do {
    status = pl180->base[MCI_STATUS];
  } while (!(status & flags));

  // Clear status flags.
  pl180->base[MCI_CLEAR] = status & flags;

  return status & err_flags;
}
