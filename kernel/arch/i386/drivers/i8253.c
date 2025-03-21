#include <kernel/time.h>

#include <arch/i386/i8253.h>
#include <arch/i386/io.h>

enum {
  PIT_DATA0  = 0x40,
  PIT_CMD    = 0x43,
};

static const unsigned PIT_FREQ = 1193182U;

#define PIT_DIV(x)    ((PIT_FREQ+(x)/2)/(x))

enum {
  PIT_SEL0     = 0x00,
  PIT_ONESHOT  = 0x01,
  PIT_RATEGEN  = 0x02,
  PIT_16BIT    = 0x30,
};

void
i8253_init_periodic(void)
{
  outb(PIT_CMD, PIT_SEL0 | PIT_RATEGEN | PIT_16BIT);

  outb(PIT_DATA0, PIT_DIV(TICKS_PER_SECOND) % 256);
  outb(PIT_DATA0, PIT_DIV(TICKS_PER_SECOND) / 256);
}

void
i8253_count_down(void)
{
  uint16_t count;

  outb(PIT_CMD, PIT_SEL0 | PIT_ONESHOT | PIT_16BIT);

  outb(PIT_DATA0, PIT_DIV(TICKS_PER_SECOND) % 256);
  outb(PIT_DATA0, PIT_DIV(TICKS_PER_SECOND) / 256);

  do {
    outb(PIT_CMD, PIT_SEL0);

    count = inb(PIT_DATA0);
    count |= inb(PIT_DATA0) << 8;
  } while (count != 0);

  outb(PIT_CMD, 0);
}
