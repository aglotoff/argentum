#include <stdint.h>

#include <argentum/drivers/console.h>
#include <argentum/mm/memlayout.h>
#include <argentum/mm/vm.h>
#include <argentum/irq.h>

#include "kbd.h"
#include "pl050.h"

// PBX-A9 has two KMIs: KMI0 is used for the keyboard and KMI1 is used for the
// mouse.
static struct Pl050 kmi0;    

static void kbd_irq(void);

/**
 * Initialize the keyboard driver.
 */
void
kbd_init(void)
{
  pl050_init(&kmi0, PA2KVA(PHYS_KMI0));

  // 0xF0 (Set Scan Code Set)
  pl050_putc(&kmi0, 0xF0);
  pl050_putc(&kmi0, 1);

  irq_attach(IRQ_KMI0, kbd_irq, 0);
}

/**
 * Handle interrupt from the keyboard.
 * 
 * Get data and store it into the console buffer.
 */
static void
kbd_irq(void)
{
  console_interrupt(kbd_getc);
}

// Keymap column indicies for different states
#define KEYMAP_COL_NORMAL   0
#define KEYMAP_COL_SHIFT    1
#define KEYMAP_COL_CTRL     2
#define KEYMAP_COL_MAX      3

// Keymap size in characters
#define KEYMAP_LENGTH       256

// Special key codes
#define KEY_HOME            0xE0
#define KEY_END             0xE1
#define KEY_UP              0xE2
#define KEY_DOWN            0xE3
#define KEY_LEFT            0xE4
#define KEY_RIGHT           0xE5
#define KEY_PGUP            0xE6
#define KEY_PGDN            0xE7
#define KEY_INSERT          0xE8

// Map scan codes to key codes
static uint8_t
key_map[KEYMAP_LENGTH][KEYMAP_COL_MAX] = {
// code     key               normal      shift   ctrl
// =====================================================
  [0x00]                    { 0,          0,      0   },
  [0x01] /* Esc         */  { 0x1B,       0x1B,   0   },
  [0x02] /* 1           */  { '1',        '!',    0   },
  [0x03] /* 2           */  { '2',        '@',    0   },
  [0x04] /* 3           */  { '3',        '#',    0   },
  [0x05] /* 4           */  { '4',        '$',    0   },
  [0x06] /* 5           */  { '5',        '%',    0   },
  [0x07] /* 6           */  { '6',        '^',    0   },
  [0x08] /* 7           */  { '7',        '&',    0   },
  [0x09] /* 8           */  { '8',        '*',    0   },
  [0x0A] /* 9           */  { '9',        '(',    0   },
  [0x0B] /* 0           */  { '0',        ')',    0   },
  [0x0C] /* -           */  { '-',        '_',    0   },
  [0x0D] /* =           */  { '=',        '+',    0   },
  [0x0E] /* Backspace   */  { '\b',       '\b',   0   },
  [0x0F] /* Tab         */  { '\t',       '\t',   0   },

  [0x10] /* Q           */  { 'q',        'Q',    C('Q')  },
  [0x11] /* W           */  { 'w',        'W',    C('W')  },
  [0x12] /* E           */  { 'e',        'E',    C('E')  },
  [0x13] /* R           */  { 'r',        'R',    C('R')  },
  [0x14] /* T           */  { 't',        'T',    C('T')  },
  [0x15] /* Y           */  { 'y',        'Y',    C('Y')  },
  [0x16] /* U           */  { 'u',        'U',    C('U')  },
  [0x17] /* I           */  { 'i',        'I',    C('I')  },
  [0x18] /* O           */  { 'o',        'O',    C('O')  },
  [0x19] /* P           */  { 'p',        'P',    C('P')  },
  [0x1A] /* [           */  { '[',        '{',    0       },
  [0x1B] /* ]           */  { ']',        '}',    0       },
  [0x1C] /* Enter       */  { '\n',       '\r',   0       },
  [0x1D] /* Left Ctrl   */  { 0,          0,      0       },
  [0x1E] /* A           */  { 'a',        'A',    C('A')  },
  [0x1F] /* S           */  { 's',        'S',    C('S')  },

  [0x20] /* D           */  { 'd',        'D',    C('D')  },
  [0x21] /* F           */  { 'f',        'F',    C('F')  },
  [0x22] /* G           */  { 'g',        'G',    C('G')  },
  [0x23] /* H           */  { 'h',        'H',    C('H')  },
  [0x24] /* J           */  { 'j',        'J',    C('J')  },
  [0x25] /* K           */  { 'k',        'K',    C('K')  },
  [0x26] /* L           */  { 'l',        'L',    C('L')  },
  [0x27] /* ;           */  { ';',        ':',    0       },
  [0x28] /* '           */  { '\'',       '"',    0       },
  [0x29] /* `           */  { '`',        '~',    0       },
  [0x2A] /* Left Shift  */  { 0,          0,      0       },
  [0x2B] /* \           */  { '\\',       '|',    0       },
  [0x2C] /* Z           */  { 'z',        'Z',    C('Z')  },
  [0x2D] /* X           */  { 'x',        'X',    C('X')  },
  [0x2E] /* C           */  { 'c',        'C',    C('C')  },
  [0x2F] /* V           */  { 'v',        'V',    C('V')  },

  [0x30] /* B           */  { 'b',        'B',    C('B')  },
  [0x31] /* N           */  { 'n',        'N',    C('N')  },
  [0x32] /* M           */  { 'm',        'M',    C('M')  },
  [0x33] /* ,           */  { ',',        '<',    0       },
  [0x34] /* .           */  { '.',        '>',    0       },
  [0x35] /* /           */  { '/',        '?',    0       },
  [0x36] /* Right Shift */  { 0,          0,      0       },
  [0x37] /* *           */  { '*',        '*',    0       },
  [0x38] /* Left Alt    */  { 0,          0,      0       },
  [0x39] /* `Space      */  { ' ',        ' ',    0       },
  [0x3A] /* Caps Lock   */  { 0,          0,      0       },
  [0x3B] /* F1          */  { 0,          0,      0       },
  [0x3C] /* F2          */  { 0,          0,      0       },
  [0x3D] /* F3          */  { 0,          0,      0       },
  [0x3E] /* F4          */  { 0,          0,      0       },
  [0x3F] /* F5          */  { 0,          0,      0       },

  [0x40] /* F6          */  { 0,          0,      0       },
  [0x41] /* F7          */  { 0,          0,      0       },
  [0x42] /* F8          */  { 0,          0,      0       },
  [0x43] /* F9          */  { 0,          0,      0       },
  [0x44] /* F10         */  { 0,          0,      0       },
  [0x45] /* Num Lock    */  { 0,          0,      0       },
  [0x46] /* Scroll Lock */  { 0,          0,      0       },
  [0x47] /* (keypad) 7  */  { KEY_HOME,   '7',    0       },
  [0x48] /* (keypad) 8  */  { KEY_UP,     '8',    0       },
  [0x49] /* (keypad) 9  */  { KEY_PGUP,   '9',    0       },
  [0x4A] /* (keypad) -  */  { '-',        '-',    0       },
  [0x4B] /* (keypad) 4  */  { KEY_LEFT,   '4',    0       },
  [0x4C] /* (keypad) 5  */  { '5',        '5',    0       },
  [0x4D] /* (keypad) 6  */  { KEY_RIGHT,  '6',    0       },
  [0x4E] /* (keypad) +  */  { '+',        '+',    0       },
  [0x4F] /* (keypad) 1  */  { KEY_END,    '1',    0       },

  [0x50] /* (keypad) 2  */  { KEY_DOWN,   '2',    0       },
  [0x51] /* (keypad) 3  */  { KEY_PGDN,   '3',    0       },
  [0x52] /* (keypad) 0  */  { KEY_INSERT, '0',    0       },
  [0x53] /* (keypad) .  */  { '.',        '.',    0       },

  [0x57] /* F11         */  { 0,          0,      0       },
  [0x58] /* F12         */  { 0,          0,      0       },
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
kbd_getc(void)
{
  // Driver state
  static int key_state;

  int scan_code, key_code, keymap_col;

  if ((scan_code = pl050_getc(&kmi0)) < 0)
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
