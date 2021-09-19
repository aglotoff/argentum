#include <stdint.h>

#include "console.h"
#include "gic.h"
#include "keymaps.h"
#include "kmi.h"
#include "memlayout.h"
#include "trap.h"
#include "vm.h"

static int kmi_read(volatile uint32_t *);
static int kmi_write(volatile uint32_t *, uint8_t);
static int kmi_kbd_getc(void);

static volatile uint32_t *kmi0;

/**
 * Initialize the keyboard driver.
 */
void
kmi_kbd_init(void)
{
  kmi0 = (volatile uint32_t *) vm_map_mmio(KMI0_BASE, 4096);

  // Select scan code set 1
  kmi_write(kmi0, 0xF0);
  kmi_write(kmi0, 1);

  // Enable interrupts.
  kmi0[KMICR] = KMICR_RXINTREN;
  gic_enable(IRQ_KMI0, 0);
}

/**
 * Handle interrupt from the keyboard.
 * 
 * Get data and store it into the console buffer.
 */
void
kmi_kbd_intr(void)
{
  console_intr(kmi_kbd_getc);
}

static int
kmi_read(volatile uint32_t *kmi)
{
  if (!(kmi[KMISTAT] & KMISTAT_RXFULL))
    return -1;
  return kmi[KMIDATA];
}

static int
kmi_write(volatile uint32_t *kmi, uint8_t data)
{
  int i;
  
  for (i = 0; i < 128 && !(kmi[KMISTAT] & KMISTAT_TXEMPTY); i++)
    ;

  kmi[KMIDATA] = data;

  while (!(kmi[KMISTAT] & KMISTAT_RXFULL))
    ;
  return kmi[KMIDATA] == 0xfa ? 0 : -1;
}

// Shift key states
#define SHIFT             (1 << 0)
#define CTRL              (1 << 1)
#define ALT               (1 << 2)

// Toggle key states
#define CAPSLOCK          (1 << 3)
#define NUMLOCK           (1 << 4)
#define SCROLLLOCK        (1 << 5)

// Beginning of an 0xE0 code sequence
#define E0SEQ             (1 << 6)

// Map scan codes to "shift" states
static uint8_t
shiftcode[256] = {
  [0x1D]  CTRL,     // Left ctrl
  [0x2A]  SHIFT,    // Left shift
  [0x36]  SHIFT,    // Right shift
  [0x38]  ALT,      // Left alt
  [0x9D]  CTRL,     // Right ctrl
  [0xB8]  ALT       // Right shift
};

// Map scan codes to "toggle" states
static uint8_t
togglecode[256] = {
  [0x3A]  CAPSLOCK,
  [0x45]  NUMLOCK,
  [0x46]  SCROLLLOCK
};

static int
kmi_kbd_getc(void)
{
  static int key_state;
  int data, c;

  if ((data = kmi_read(kmi0)) < 0)
    return data;

  data = kmi0[KMIDATA];

  if (data == 0xE0) {
    // Beginning of a 0xE0 code sequence
    key_state |= E0SEQ;
    return 0;
  }
  
  if (data & 0x80) {
    // Key released
    data = (key_state & E0SEQ ? data : data & 0x7F);
    key_state &= ~(shiftcode[data] | E0SEQ);
    return 0;
  }

  if (key_state & E0SEQ) {
    // Map the code sequences beginning with 0xE0 to key codes above 127
    data |= 0x80;
    key_state &= ~E0SEQ;
  }

  key_state |= shiftcode[data];
  key_state ^= togglecode[data];

  c = keymaps[key_state & (CTRL | SHIFT)][data];

  if (key_state & CAPSLOCK) {
    if (c >= 'a' && c <= 'z') {
      c += 'A' - 'a';
    } else if (c >= 'A' && c <= 'Z') {
      c += 'a' - 'A';
    }
  }

  return c;
}
