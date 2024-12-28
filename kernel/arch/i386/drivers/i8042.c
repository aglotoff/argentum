#include <arch/i386/io.h>
#include <arch/i386/i8042.h>

static void i8042_kbd_putc(void *, char);
static int  i8042_kbd_getc(void *);

static struct PS2Ops i8042_ops = {
  .getc = i8042_kbd_getc,
  .putc = i8042_kbd_putc,
};

#define I8042_DATA          0x60        // Data Port (RW)
#define I8042_STATUS        0x64        // Status Register (R)
#define   I8042_OUTPUT_FULL   (1 << 0)
#define   I8042_INPUT_FULL    (1 << 1)
#define I8042_COMMAND       0x64        // Command Register (W)

int
i8042_init(struct I8042 *i8042, int irq)
{
  return ps2_init(&i8042->ps2, &i8042_ops, i8042, irq);
}

static void
i8042_kbd_putc(void *, char)
{
  // TODO
}

static int
i8042_kbd_getc(void *)
{
  // Check whether the receive register is full.
  if (!(inb(I8042_STATUS) & I8042_OUTPUT_FULL))
    return -1;

  return inb(I8042_DATA);
}

int
i8042_getc(struct I8042 *i8042)
{
  return ps2_kbd_getc(&i8042->ps2);
}
