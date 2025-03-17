#include <stdint.h>

#include <kernel/tty.h>
#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/trap.h>
#include <kernel/interrupt.h>

#include <kernel/drivers/ps2.h>
#include <kernel/drivers/kbd.h>

void ps2_kbd_irq_task(int, void *);

int
ps2_init(struct PS2 *ps2, struct PS2Ops *ops, void *ops_arg, int irq)
{
  ps2->ops     = ops;
  ps2->ops_arg = ops_arg;

  interrupt_attach_task(irq, ps2_kbd_irq_task, ps2);

  return 0;
}

static char *key_sequences[KEY_MAX] = {
  [KEY_UP]      "\033[A",
  [KEY_DOWN]    "\033[B",
  [KEY_RIGHT]   "\033[C",
  [KEY_LEFT]    "\033[D",
  [KEY_HOME]    "\033[H",
  [KEY_INSERT]  "\033[L",
  [KEY_BTAB]    "\033[Z",
};

// Keymap size in characters
#define KEYMAP_LENGTH       256

/**
 * Handle interrupt from the keyboard.
 * 
 * Get data and store it into the console buffer.
 */
void
ps2_kbd_irq_task(int irq, void *arg)
{
  struct PS2 *ps2 = (struct PS2 *) arg;
  char buf[2];
  int c;

  while ((c = ps2_kbd_getc(ps2)) >= 0) {
    if ((c == 0) || (c >= KEY_MAX))
      continue;

    if (key_sequences[c] != NULL) {
      tty_process_input(tty_current, key_sequences[c]);
    } else {
      buf[0] = c & 0xFF;
      buf[1] = '\0';
      tty_process_input(tty_current, buf);
    }
  }

  arch_interrupt_unmask(irq);
}

// Keymap column indicies for different states
#define KEYMAP_COL_NORMAL   0
#define KEYMAP_COL_SHIFT    1
#define KEYMAP_COL_CTRL     2
#define KEYMAP_COL_MAX      3

// Map scan codes to key codes
static uint16_t
key_map[KEYMAP_LENGTH][KEYMAP_COL_MAX] = {
// code     key               normal      shift     ctrl
// =====================================================
  [0x00]                   { 0,           0,        0   },
  [0x01] /* Esc         */ { 0x1B,        0x1B,     0   },
  [0x02] /* 1            */ { '1',        '!',      0   },
  [0x03] /* 2            */ { '2',        '@',      0   },
  [0x04] /* 3            */ { '3',        '#',      0   },
  [0x05] /* 4            */ { '4',        '$',      0   },
  [0x06] /* 5            */ { '5',        '%',      0   },
  [0x07] /* 6            */ { '6',        '^',      0   },
  [0x08] /* 7            */ { '7',        '&',      0   },
  [0x09] /* 8            */ { '8',        '*',      0   },
  [0x0A] /* 9            */ { '9',        '(',      0   },
  [0x0B] /* 0            */ { '0',        ')',      0   },
  [0x0C] /* -            */ { '-',        '_',      0   },
  [0x0D] /* =            */ { '=',        '+',      0   },
  [0x0E] /* Backspace    */ { '\b',       '\b',     0   },
  [0x0F] /* Tab          */ { '\t',       KEY_BTAB, 0   },

  [0x10] /* Q            */ { 'q',        'Q',      C('Q')  },
  [0x11] /* W            */ { 'w',        'W',      C('W')  },
  [0x12] /* E            */ { 'e',        'E',      C('E')  },
  [0x13] /* R            */ { 'r',        'R',      C('R')  },
  [0x14] /* T            */ { 't',        'T',      C('T')  },
  [0x15] /* Y            */ { 'y',        'Y',      C('Y')  },
  [0x16] /* U            */ { 'u',        'U',      C('U')  },
  [0x17] /* I            */ { 'i',        'I',      C('I')  },
  [0x18] /* O            */ { 'o',        'O',      C('O')  },
  [0x19] /* P            */ { 'p',        'P',      C('P')  },
  [0x1A] /* [            */ { '[',        '{',      0       },
  [0x1B] /* ]            */ { ']',        '}',      0       },
  [0x1C] /* Enter        */ { '\n',       '\r',     0       },
  [0x1D] /* Left Ctrl    */ { 0,          0,        0       },
  [0x1E] /* A            */ { 'a',        'A',      C('A')  },
  [0x1F] /* S            */ { 's',        'S',      C('S')  },

  [0x20] /* D            */ { 'd',        'D',      C('D')  },
  [0x21] /* F            */ { 'f',        'F',      C('F')  },
  [0x22] /* G            */ { 'g',        'G',      C('G')  },
  [0x23] /* H            */ { 'h',        'H',      C('H')  },
  [0x24] /* J            */ { 'j',        'J',      C('J')  },
  [0x25] /* K            */ { 'k',        'K',      C('K')  },
  [0x26] /* L            */ { 'l',        'L',      C('L')  },
  [0x27] /* ;            */ { ';',        ':',      0       },
  [0x28] /* '            */ { '\'',       '"',      0       },
  [0x29] /* `            */ { '`',        '~',      0       },
  [0x2A] /* Left Shift   */ { 0,          0,        0       },
  [0x2B] /* \            */ { '\\',       '|',      C('\\') },
  [0x2C] /* Z            */ { 'z',        'Z',      C('Z')  },
  [0x2D] /* X            */ { 'x',        'X',      C('X')  },
  [0x2E] /* C            */ { 'c',        'C',      C('C')  },
  [0x2F] /* V            */ { 'v',        'V',      C('V')  },

  [0x30] /* B            */ { 'b',        'B',      C('B')  },
  [0x31] /* N            */ { 'n',        'N',      C('N')  },
  [0x32] /* M            */ { 'm',        'M',      C('M')  },
  [0x33] /* ,            */ { ',',        '<',      0       },
  [0x34] /* .            */ { '.',        '>',      0       },
  [0x35] /* /            */ { '/',        '?',      0       },
  [0x36] /* Right Shift  */ { 0,          0,        0       },
  [0x37] /* *            */ { '*',        '*',      0       },
  [0x38] /* Left Alt     */ { 0,          0,        0       },
  [0x39] /* `Space       */ { ' ',        ' ',      0       },
  [0x3A] /* Caps Lock    */ { 0,          0,        0       },
  [0x3B] /* F1           */ { 0,          0,        0       },
  [0x3C] /* F2           */ { 0,          0,        0       },
  [0x3D] /* F3           */ { 0,          0,        0       },
  [0x3E] /* F4           */ { 0,          0,        0       },
  [0x3F] /* F5           */ { 0,          0,        0       },

  [0x40] /* F6           */ { 0,          0,        0       },
  [0x41] /* F7           */ { 0,          0,        0       },
  [0x42] /* F8           */ { 0,          0,        0       },
  [0x43] /* F9           */ { 0,          0,        0       },
  [0x44] /* F10          */ { 0,          0,        0       },
  [0x45] /* Num Lock     */ { 0,          0,        0       },
  [0x46] /* Scroll Lock  */ { 0,          0,        0       },
  [0x47] /* (keypad) 7   */ { KEY_HOME,   '7',      0       },
  [0x48] /* (keypad) 8   */ { KEY_UP,     '8',      0       },
  [0x49] /* (keypad) 9   */ { 0,          '9',      0       },
  [0x4A] /* (keypad) -   */ { '-',        '-',      0       },
  [0x4B] /* (keypad) 4   */ { KEY_LEFT,   '4',      0       },
  [0x4C] /* (keypad) 5   */ { '5',        '5',      0       },
  [0x4D] /* (keypad) 6   */ { KEY_RIGHT,  '6',      0       },
  [0x4E] /* (keypad) +   */ { '+',        '+',      0       },
  [0x4F] /* (keypad) 1   */ { 0,          '1',      0       },

  [0x50] /* (keypad) 2   */ { KEY_DOWN,   '2',      0       },
  [0x51] /* (keypad) 3   */ { 0,          '3',      0       },
  [0x52] /* (keypad) 0   */ { KEY_INSERT, '0',      0       },
  [0x53] /* (keypad) .   */ { '.',        '.',      0       },

  [0x57] /* F11          */ { 0,          0,        0       },
  [0x58] /* F12          */ { 0,          0,        0       },

  [0xC8] /* cursor up    */ { KEY_UP,     'A',      0       },
  [0xCB] /* cursor left  */ { KEY_LEFT,   'D',      0       },
  [0xCD] /* cursor right */ { KEY_RIGHT,  'C',      0       },
  [0xD0] /* cursor down  */ { KEY_DOWN,   'B',      0       },
};

// Driver states
#define STATE_SHIFT         (1 << 0)  // Left Shift pressed
#define STATE_CTRL          (1 << 1)  // Right Ctrl pressed
#define STATE_ALT           (1 << 2)  // Left Alt pressed
#define STATE_CAPS_LOCK     (1 << 3)  // Caps Lock active
#define STATE_NUM_LOCK      (1 << 4)  // Num Lock active
#define STATE_SCROLL_LOCK   (1 << 5)  // Scroll Lock active
#define STATE_E0_ESC        (1 << 6)  // E0 byte detected

// Map scan codes to "shiftable" states
static uint8_t
shift_map[KEYMAP_LENGTH] = {
  [0x1D]  STATE_CTRL,         // Left / Right Ctrl
  [0x2A]  STATE_SHIFT,        // Left Shift
  [0x36]  STATE_SHIFT,        // Right Shift
  [0x38]  STATE_ALT,          // Left / Right Alt
};

// Map scan codes to "toggleable" states
static uint8_t
toggle_map[KEYMAP_LENGTH] = {
  [0x3A]  STATE_CAPS_LOCK,    // Caps Lock
  [0x45]  STATE_NUM_LOCK,     // Num Lock
  [0x46]  STATE_SCROLL_LOCK,  // Sroll Lock
};

int
ps2_kbd_getc(struct PS2 *ps2)
{
  // Driver state
  static int key_state;

  int scan_code, key_code, keymap_col;

  if ((scan_code = ps2->ops->getc(ps2->ops_arg)) < 0)
    return scan_code;

  // Beginning of a E0 code sequence.
  if (scan_code == 0xE0) {
    key_state |= STATE_E0_ESC;
    return 0;
  }

  // Key released.
  if (scan_code & 0x80) {
    key_state &= ~(shift_map[scan_code & 0x7F] | STATE_E0_ESC);
    return 0;
  }

  // Map code sequences beginning with E0 to key codes above 127.
  if (key_state & STATE_E0_ESC) {
    scan_code |= 0x80;
    key_state &= ~STATE_E0_ESC;
  }

  // Update state.
  key_state |= shift_map[scan_code];
  key_state ^= toggle_map[scan_code];

  // Alt + F1..F6 pressed
  if ((key_state & STATE_ALT) && (scan_code >= 0x3B) && (scan_code <= 0x40)) {
    tty_switch(scan_code - 0x3B);
    return 0;
  }

  // TODO: may need more columns: Alt, Ctrl + Alt, Alt + Shift, etc.
  if (key_state & STATE_CTRL)
    keymap_col = KEYMAP_COL_CTRL;
  else if (key_state & STATE_SHIFT)
    keymap_col = KEYMAP_COL_SHIFT;
  else
    keymap_col = KEYMAP_COL_NORMAL;

  key_code = key_map[scan_code][keymap_col];

  // Make letters uppercase.
  if (key_state & STATE_CAPS_LOCK) {
    if ((key_code >= 'a') && (key_code <= 'z')) {
      key_code += 'A' - 'a';
    } else if ((key_code >= 'A') && (key_code <= 'Z')) {
      key_code += 'a' - 'A';
    }
  }

  return key_code;
}
