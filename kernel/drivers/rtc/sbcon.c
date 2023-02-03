#include "sbcon.h"

/*******************************************************************************
 * Two-wire serial bus interface driver.
 * 
 * Note: this code works in QEMU but hasn't been tested on real hardware!
 * 
 * For more info on I2C programming, see this tutorial:
 * https://www.robot-electronics.co.uk/i2c-tutorial
 ******************************************************************************/

// Control registers, divided by 4 for use as uint32_t[] indices
enum {
  SB_CONTROL  = (0x000 / 4),   // Read serial control bits
  SB_CONTROLS = (0x000 / 4),   // Set serial control bits
  SB_CONTROLC = (0x004 / 4),   // Clear serial control bits
};

// Control registers bits
enum {
  SCL = (1U << 0),      // Clock line
  SDA = (1U << 1),      // Data line
};

static void    sbcon_start(struct SBCon *);
static void    sbcon_stop(struct SBCon *);
static uint8_t sbcon_rx_byte(struct SBCon *, int);
static int     sbcon_tx_byte(struct SBCon *, uint8_t);

static void
sbcon_delay(void)
{
  // TODO: must wait on real hardware
}

static void
sbcon_out(struct SBCon *i2c, int line, int high)
{
  i2c->base[high ? SB_CONTROLS : SB_CONTROLC] = line;
}

static int
sbcon_in(struct SBCon *i2c, int line)
{
  return i2c->base[SB_CONTROL] & line;
}

/**
 * Initialize the serial bus driver.
 *
 * @param i2c Pointer to the driver instance to initialize.
 */
void
sbcon_init(struct SBCon *i2c, void *base)
{
  i2c->base = (volatile uint32_t *) base;

  // Release both lines
  sbcon_out(i2c, SDA, 1);
  sbcon_out(i2c, SCL, 1);
}

/**
 * Read from a slave device.
 * 
 * @param i2c   Pointer to the I2C bus driver intstance
 * @param addr  The device address
 * @param reg   Internal device register address
 * @param buf   Pointer to the block of memory to store the data.
 * @param n     The number of data bytes to read.
 *
 * @return 0 on success, a non-zero value on error.
 */
int
sbcon_read(struct SBCon *i2c, uint8_t addr, uint8_t reg, void *buf, size_t n)
{
  uint8_t *data = (uint8_t *) buf;

  sbcon_start(i2c);               // Generate a start condition
  sbcon_tx_byte(i2c, addr);       // Send device write address (R/W bit low)
  sbcon_tx_byte(i2c, reg);        // Send the internal device register number

  sbcon_start(i2c);               // Repeated start
  sbcon_tx_byte(i2c, addr | 0x1); // Send device read address (R/W bit high)

  // Receive data; acknowledge all bytes except the last one
  while (n-- > 0)
    *data++ = sbcon_rx_byte(i2c, n == 0);   

  // Generate a stop condition
  sbcon_stop(i2c);

  return 0;
}

/**
 * Write to a slave device.
 * 
 * @param i2c   Pointer to the I2C bus driver intstance
 * @param addr  The device address
 * @param reg   Internal device register address
 * @param buf   Pointer to the block of memory containing the data.
 * @param n     The number of data bytes to write.
 *
 * @return 0 on success, a non-zero value on error.
 */
int
sbcon_write(struct SBCon *i2c, uint8_t addr, uint8_t reg, const void *buf,
            size_t n)
{
  const uint8_t *data = (const uint8_t *) buf;

  sbcon_start(i2c);               // Generate a start condition
  sbcon_tx_byte(i2c, addr);       // Send device write address (R/W bit low)
  sbcon_tx_byte(i2c, reg);        // Send the internal device register number

  // Transmit data
  while (n-- > 0)
    sbcon_tx_byte(i2c, *data++);

  // Generate a stop condition
  sbcon_stop(i2c);

  return 0;
}

// START condition: change SDA from high to low while keeping SCL high
static void
sbcon_start(struct SBCon *i2c)
{
  sbcon_out(i2c, SDA, 1);
  sbcon_out(i2c, SCL, 1);
  sbcon_delay();

  sbcon_out(i2c, SDA, 0);
  sbcon_delay();

  sbcon_out(i2c, SCL, 0);
  sbcon_delay();
}

// STOP condition: change SDA from low to high while keeping SCL high
static void
sbcon_stop(struct SBCon *i2c)
{
  sbcon_out(i2c, SDA, 0);
  sbcon_out(i2c, SCL, 0);
  sbcon_delay();

  sbcon_out(i2c, SDA, 1);
  sbcon_delay();

  sbcon_out(i2c, SCL, 1);
  sbcon_delay();
}

// Receive 8 bits of data and send back a (N)ACK bit
static uint8_t
sbcon_rx_byte(struct SBCon *i2c, int ack)
{
  uint8_t data = 0;
  int i;

  // Release the SDA line so the slave can take control of it
  sbcon_out(i2c, SDA, 1);
  sbcon_delay();

  for (i = 0; i < 8; i++) {
    sbcon_out(i2c, SCL, 1);
    sbcon_delay();

    // Wait if clock stretching takes place (the slave is holding SCL low)
    while (!sbcon_in(i2c, SCL))
      ;

    // Receive the next most significant byte
    data <<= 1;
    if (sbcon_in(i2c, SDA))
      data |= 1;

    sbcon_out(i2c, SCL, 0);
    sbcon_delay();
  }

  // Place the N(ACK) bit on the SDA line
  sbcon_out(i2c, SDA, ack);

  // Pulse the SCL line high, then low to synchronize
  sbcon_out(i2c, SCL, 1);
  sbcon_delay();
  sbcon_out(i2c, SCL, 0);
  sbcon_delay();

  return data;
}

// Transmit 8 bits of data and receive a (N)ACK bit
static int
sbcon_tx_byte(struct SBCon *i2c, uint8_t data)
{
  int i, ack;

  for (i = 0; i < 8; i++) {
    // Place the next most significant bit on the SDA line 
    sbcon_out(i2c, SDA, data & 0x80);

    // Pulse the SCL line high, then low to synchronize
    sbcon_out(i2c, SCL, 1);
    sbcon_delay();
    sbcon_out(i2c, SCL, 0);
    sbcon_delay();

    data <<= 1;
  }

  // Release the SDA line so the slave can take control of it
  sbcon_out(i2c, SDA, 1);
  sbcon_delay();

  // Receive the (N)ACK bit
  sbcon_out(i2c, SCL, 1);
  sbcon_delay();

  ack = sbcon_in(i2c, SDA);

  sbcon_out(i2c, SCL, 0);
  sbcon_delay();

  return ack;
}
