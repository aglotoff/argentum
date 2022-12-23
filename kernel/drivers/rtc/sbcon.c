// For more info on I2C programming, see this tutorial:
// https://www.robot-electronics.co.uk/i2c-tutorial
 
#include "sbcon.h"

#define SCL           (1U << 0)     // Clock line
#define SDA           (1U << 1)     // Data line

// Serial bus registers, divided by 4 for use as uint32_t[] indices
#define SB_CONTROL    (0x000 / 4)   // Read serial control bits
#define SB_CONTROLS   (0x000 / 4)   // Set serial control bits
#define SB_CONTROLC   (0x004 / 4)   // Clear serial control bits

void
sbcon_init(struct SBCon *i2c, void *base)
{
  i2c->base = (volatile uint32_t *) base;

  i2c->base[SB_CONTROLS] = SCL;
  i2c->base[SB_CONTROLS] = SDA;
}

static void sbcon_delay(void);
static void sbcon_start(struct SBCon *);
static void sbcon_stop(struct SBCon *);
static int  sbcon_rx_byte(struct SBCon *, int);
static int  sbcon_tx_byte(struct SBCon *, char);

// Read from a slave device
int
sbcon_read(struct SBCon *i2c, uint8_t addr, uint8_t reg)
{
  uint8_t data;

  sbcon_start(i2c);               // Send a start sequence
  sbcon_tx_byte(i2c, addr);       // Send device write address (R/W bit low)
  sbcon_tx_byte(i2c, reg);        // Send the internal register number
  sbcon_start(i2c);               // Send a start sequence again
  sbcon_tx_byte(i2c, addr | 0x1); // Send device read address (R/W bit high)
  data = sbcon_rx_byte(i2c, 1);   // Read data byte
  sbcon_stop(i2c);                // Send the stop sequence

  return data;
}

// Dummy delay routine
static void
sbcon_delay(void)
{
  // TODO: do something
}

// Send the start sequence
static void
sbcon_start(struct SBCon *i2c)
{
  i2c->base[SB_CONTROLS] = SDA;
  sbcon_delay();
  i2c->base[SB_CONTROLS] = SCL;
  sbcon_delay();
  i2c->base[SB_CONTROLC] = SDA;
  sbcon_delay();
  i2c->base[SB_CONTROLC] = SCL;
  sbcon_delay();
}

// Send the stop sequence
static void
sbcon_stop(struct SBCon *i2c)
{
  i2c->base[SB_CONTROLC] = SDA;
  sbcon_delay();
  i2c->base[SB_CONTROLS] = SCL;
  sbcon_delay();
  i2c->base[SB_CONTROLS] = SDA;
  sbcon_delay();
}

// Receive 8 bits of data
static int
sbcon_rx_byte(struct SBCon *i2c, int ack)
{
  uint8_t x, data = 0;

  i2c->base[SB_CONTROLS] = SDA;

  for (x = 0; x < 8; x++) {
    data <<= 1;

    do {
      i2c->base[SB_CONTROLS] = SCL;
    } while (!(i2c->base[SB_CONTROL] >> (SCL-1)) & 0x1);
    sbcon_delay();

    if ((i2c->base[SB_CONTROL] >> (SDA-1)) & 0x1) {
      data |= 1;
    }
    i2c->base[SB_CONTROLC] = SCL;
  }

  // Send (N)ACK bit
  if (ack) {
    i2c->base[SB_CONTROLS] = SDA;
  } else {
    i2c->base[SB_CONTROLC] = SDA;
  }
  i2c->base[SB_CONTROLS] = SCL;
  sbcon_delay();

  i2c->base[SB_CONTROLC] = SCL;
  i2c->base[SB_CONTROLS] = SDA;

  return data;
}

// Transmit 8 bits of data
static int
sbcon_tx_byte(struct SBCon *i2c, char data)
{
  uint8_t bit, x;

  for (x = 0; x < 8; x++) {
    if (data & 0x80) {
      i2c->base[SB_CONTROLS] = SDA;
    } else {
      i2c->base[SB_CONTROLC] = SDA;
    }

    i2c->base[SB_CONTROLS] = SCL;
    sbcon_delay();
    i2c->base[SB_CONTROLC] = SCL;

    data <<= 1;
  }

  // Possible ACK bit
  i2c->base[SB_CONTROLS] = SDA;
  i2c->base[SB_CONTROLS] = SCL;
  bit = (i2c->base[SB_CONTROL] >> (SDA-1)) & 0x1;
  i2c->base[SB_CONTROLC] = SCL;

  return bit;
}
