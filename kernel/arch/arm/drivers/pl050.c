// See ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050) Technical Reference
// Manual.

#include <arch/arm/pl050.h>

// KMI registers
#define KMI_CR            0x000   // Control register
#define   KMICR_RXINTREN    (1 << 4)   // Enable receiver interrupt
#define KMI_STAT          0x004   // Status register
#define   RXFULL            (1 << 4)   // Receiver register full
#define   TXEMPTY           (1 << 6)   // Transmit register empty
#define KMI_DATA          0x008   // Received data / data to be transmitted

static void pl050_putc(void *, char);
static int  pl050_getc(void *);

static inline uint32_t
pl050_read(struct Pl050 *pl050, uint32_t reg)
{
  return pl050->base[reg >> 2];
}

static inline void
pl050_write(struct Pl050 *pl050, uint32_t reg, uint32_t data)
{
  pl050->base[reg >> 2] = data;
}

static struct PS2Ops pl050_ops = {
  .getc = pl050_getc,
  .putc = pl050_putc,
};

/**
 * Initialize the KMI driver.
 *
 * @param pl050 Pointer to the driver instance.
 * @param base Memory base address.
 */
int
pl050_init(struct Pl050 *pl050, void *base, int irq)
{
  pl050->base = (volatile uint32_t *) base;

  // Enable interrupts
  pl050_write(pl050, KMI_CR, KMICR_RXINTREN);

  return ps2_init(&pl050->ps2, &pl050_ops, pl050, irq);
}

/**
 * Output character to the KMI device.
 * 
 * @param pl050 Pointer to the driver instance.
 * @param c The character to be printed.
 */
static void
pl050_putc(void *arg, char c)
{
  struct Pl050 *pl050 = (struct Pl050 *) arg;

  // Wait for the transmit register to become empty
  while (!(pl050_read(pl050, KMI_STAT) & TXEMPTY))
    ;

  pl050_write(pl050, KMI_DATA, c);
}

/**
 * Read character from the KMI device.
 * 
 * @param pl050 Pointer to the driver instance.
 * @return The next iput character or -1 if there is none.
 */
static int
pl050_getc(void *arg)
{
  struct Pl050 *pl050 = (struct Pl050 *) arg;

  // Check whether the receive register is full.
  if (!(pl050_read(pl050, KMI_STAT) & RXFULL))
    return -1;

  return pl050_read(pl050, KMI_DATA);
}

int
pl050_kbd_getc(struct Pl050 *pl050)
{
  return ps2_kbd_getc(&pl050->ps2);
}
