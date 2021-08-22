#include <stdint.h>

#include "console.h"
#include "kmi.h"

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
keycode[4] = {
  normalmap,
  shiftmap,
  ctrlmap,
  ctrlmap,
};

static volatile uint32_t *kmi;

static int key_state;

static int
kmi_getc(void)
{
  uint8_t data;
  int c;

  if (!(kmi[KMISTAT] & KMISTAT_RXFULL))
    return -1;

  data = kmi[KMIDATA];

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

  c = keycode[key_state & (CTRL | SHIFT)][data];

  if (key_state & CAPSLOCK) {
    if (c >= 'a' && c <= 'z') {
      c += 'A' - 'a';
    } else if (c >= 'A' && c <= 'Z') {
      c += 'a' - 'A';
    }
  }

  return c;
}

void
kmi_init(void)
{
  // TODO: map to a reserved region in the virtual address space.
  kmi = (volatile uint32_t *) KMI0;
}

void
kmi_intr(void)
{
  // Store the available data in the console buffer.
  console_intr(kmi_getc);
}
