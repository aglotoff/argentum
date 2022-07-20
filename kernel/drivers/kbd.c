// See ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050) Technical Reference
// Manual.

#include <stdint.h>

#include <drivers/console.h>
#include <drivers/gic.h>
#include <mm/memlayout.h>
#include <mm/vm.h>
#include <trap.h>

#include <drivers/kbd.h>

// PBX-A9 has two KMIs: KMI0 is used for the keyboard and KMI1 is used for the
// mouse.
#define KMI0_BASE         0x10006000        

// KMI registers, shifted right by 2 bits for use as uint32_t[] indices
#define KMICR             (0x000 / 4)   // Control register
#define   KMICR_RXINTREN    (1U << 4)   // Enable receiver interrupt
#define KMISTAT           (0x004 / 4)   // Status register
#define   KMISTAT_RXFULL    (1U << 4)   // Receiver register full
#define   KMISTAT_TXEMPTY   (1U << 6)   // Transmit register empty
#define KMIDATA           (0x008 / 4)   // Received data

static int kmi_read(volatile uint32_t *);
static int kmi_write(volatile uint32_t *, uint8_t);

static volatile uint32_t *kmi0;

/**
 * Initialize the keyboard driver.
 */
void
kbd_init(void)
{
  kmi0 = (volatile uint32_t *) KADDR(KMI0_BASE);

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
kbd_intr(void)
{
  console_intr(kbd_getc);
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

// Map scan codes in the "normal" state to key codes
static uint8_t
normalmap[256] =
{
  '\0', 0x1B, '1',  '2',  '3',  '4',  '5',  '6',    // 0x00
  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',    // 0x10
  'o',  'p',  '[',  ']',  '\n', '\0', 'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',    // 0x20
  '\'', '`',  '\0', '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  '\0', '*',    // 0x30
  '\0', ' ',  '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',    // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  '\0', '\0', '\0', '\0',   // 0x50
  [0x9C]  '\n',
  [0xB5]  '/',
};

// Map scan codes in the "shift" state to key codes
static uint8_t
shiftmap[256] =
{
  '\0', 033,  '!',  '@',  '#',  '$',  '%',  '^',    // 0x00
  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',    // 0x10
  'O',  'P',  '{',  '}',  '\n', '\0',  'A', 'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',    // 0x20
  '"',  '~',  '\0',  '|',  'Z',  'X',  'C', 'V',
  'B',  'N',  'M',  '<',  '>',  '?',  '\0', '*',    // 0x30
  '\0', ' ',  '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',    // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  '\0', '\0', '\0', '\0',   // 0x50
  [0x9C]  '\n',
  [0xB5]  '/',
};

// Map scan codes in the "ctrl" state to key codes
static uint8_t
ctrlmap[256] =
{
  '\0',   '\0',   '\0',   '\0',     '\0',   '\0',   '\0',   '\0',     // 0x00
  '\0',   '\0',   '\0',   '\0',     '\0',   '\0',   '\0',   '\0',
  C('Q'), C('W'), C('E'), C('R'),   C('T'), ('Y'),  C('U'), C('I'),   // 0x10
  C('O'), C('P'), '\0',   '\0',     '\r',   '\0',   C('A'), C('S'),
  C('D'), C('F'), C('G'), C('H'),   C('J'), C('K'), C('L'), '\0',     // 0x20
  '\0',   '\0',   '\0',   C('\\'),  C('Z'), C('X'), C('C'), C('V'),
  C('B'), C('N'), C('M'), '\0',    '\0',    C('/'), '\0',   '\0',     // 0x30
  [0x9C]  '\r',
  [0xB5]  C('/'),
};

// Map scan codes to key codes
static uint8_t *
keymaps[4] = {
  normalmap,
  shiftmap,
  ctrlmap,
  ctrlmap,
};

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

int
kbd_getc(void)
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
