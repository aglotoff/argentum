// See ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050) Technical Reference
// Manual.

#include <drivers/pl050.h>

// KMI registers, shifted right by 2 bits for use as uint32_t[] indices
#define KMICR             (0x000 / 4)   // Control register
#define   KMICR_RXINTREN    (1U << 4)   // Enable receiver interrupt
#define KMISTAT           (0x004 / 4)   // Status register
#define   KMISTAT_RXFULL    (1U << 4)   // Receiver register full
#define   KMISTAT_TXEMPTY   (1U << 6)   // Transmit register empty
#define KMIDATA           (0x008 / 4)   // Received data

/**
 * Initialize the KMI driver.
 *
 * @param pl011 Pointer to the driver instance.
 * @param base Memory base address.
 */
int
pl050_init(struct Pl050 *pl050, void *base)
{
  pl050->base = (volatile uint32_t *) base;
  
  // Enable interrupts.
  pl050->base[KMICR] = KMICR_RXINTREN;

  return 0;
}

/**
 * Output character to the KMI device.
 * 
 * @param pl011 Pointer to the driver instance.
 * @param c The character to be printed.
 */
void
pl050_putc(struct Pl050 *pl050, char c)
{
  // Wait for the transmit register to become empty
  while (!(pl050->base[KMISTAT] & KMISTAT_TXEMPTY))
    ;

  pl050->base[KMIDATA] = c;
}

/**
 * Read character from the KMI device.
 * 
 * @param pl011 Pointer to the driver instance.
 * @return The next iput character or -1 if there is none.
 */
int
pl050_getc(struct Pl050 *pl050)
{
  // Check whether the receive register is full.
  if (!(pl050->base[KMISTAT] & KMISTAT_RXFULL))
    return -1;

  return pl050->base[KMIDATA];
}
