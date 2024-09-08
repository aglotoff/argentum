#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/drivers/console.h>
#include <kernel/trap.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/vmspace.h>
#include <kernel/fs/fs.h>
#include <kernel/cprintf.h>
#include <kernel/tick.h>
#include <kernel/mach.h>
#include <kernel/dev.h>

#include <kernel/drivers/kbd.h>
#include <kernel/drivers/display.h>

static struct CharDev tty_device = {
  .read   = tty_read,
  .write  = tty_write,
  .ioctl  = tty_ioctl,
  .select = tty_select,
};

#define NCONSOLES   6   // The total number of virtual consoles

static struct KSpinLock console_lock;
static struct Tty consoles[NCONSOLES];

struct Tty *console_current;
struct Tty *console_system;

#define IN_EOF  (1 << 0)
#define IN_EOL  (1 << 1)

// Send a signal to all processes in the given console process group
static void
tty_signal(struct Tty *console, int signo)
{
  if (console->pgrp <= 1)
    return;

  k_spinlock_release(&console->in.lock);

  if (signal_generate(-console->pgrp, signo, 0) != 0)
    panic("cannot generate signal");

  k_spinlock_acquire(&console->in.lock);
}

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  struct Tty *console;

  k_spinlock_init(&console_lock, "console");

  for (console = consoles; console < &consoles[NCONSOLES]; console++) {
    unsigned i;

    k_spinlock_init(&console->in.lock, "console.in");
    k_waitqueue_init(&console->in.queue);

    console->out.fg_color      = COLOR_WHITE;
    console->out.bg_color      = COLOR_BLACK;
    console->out.state         = PARSER_NORMAL;
    console->out.esc_cur_param = -1;
    console->out.cols          = CONSOLE_COLS;
    console->out.rows          = CONSOLE_ROWS;
    k_spinlock_init(&console->out.lock, "console.out");

    for (i = 0; i < console->out.cols * console->out.rows; i++) {
      console->out.buf[i].ch = ' ';
      console->out.buf[i].fg = COLOR_WHITE;
      console->out.buf[i].bg = COLOR_BLACK;
    }

    console->termios.c_iflag = BRKINT | ICRNL | IXON | IXANY;
    console->termios.c_oflag = OPOST | ONLCR;
    console->termios.c_cflag = CREAD | CS8 | HUPCL;
    console->termios.c_lflag = ISIG | ICANON | ECHO | ECHOE;
    console->termios.c_cc[VEOF]   = C('D');
    console->termios.c_cc[VEOL]   = _POSIX_VDISABLE;
    console->termios.c_cc[VERASE] = C('H');
    console->termios.c_cc[VINTR]  = C('C');
    console->termios.c_cc[VKILL]  = C('U');
    console->termios.c_cc[VMIN]   = 1;
    console->termios.c_cc[VQUIT]  = C('\\');
    console->termios.c_cc[VTIME]  = 0;
    console->termios.c_cc[VSUSP]  = C('Z');
    console->termios.c_cc[VSTART] = C('Q');
    console->termios.c_cc[VSTOP]  = C('S');
    console->termios.c_ispeed = B9600;
    console->termios.c_ospeed = B9600;
  }

  console_current = &consoles[0];
  console_system  = &consoles[0];

  mach_current->console_init();
  mach_current->display_update(console_current);

  dev_register_char(0x01, &tty_device);
}

static void
tty_flush(struct Tty *tty)
{
  if (tty == console_current) {
    mach_current->display_flush(tty);
    mach_current->display_update_cursor(tty);
  }
}

// Use the device minor number to select the virtual console corresponding to
// this inode
static struct Tty *
tty_from_dev(dev_t dev)
{
  // No need to lock since rdev cannot change once we obtain an inode ref
  // TODO: are we sure?
  int minor = dev & 0xFF;
  return minor < NCONSOLES ? &consoles[minor] : NULL;
}

static void
console_erase(struct Tty *console, unsigned from, unsigned to)
{
  size_t i;

  for (i = from; i <= to; i++) {
    console->out.buf[i].ch = ' ';
    console->out.buf[i].fg = console->out.fg_color;
    console->out.buf[i].bg = console->out.bg_color;
  }

  if (console == console_current)
    mach_current->display_erase(console, from, to);
}

static void
console_set_char(struct Tty *console, unsigned i, char c)
{
  console->out.buf[i].ch = c;
  console->out.buf[i].fg = console->out.fg_color & 0xF;
  console->out.buf[i].bg = console->out.bg_color & 0xF;
}

#define DISPLAY_TAB_WIDTH  4

static void
console_scroll_down(struct Tty *console, unsigned n)
{
  unsigned i;

  if (console == console_current)
    mach_current->display_flush(console);

  memmove(&console->out.buf[0], &console->out.buf[console->out.cols * n],
          sizeof(console->out.buf[0]) * (console->out.cols * (console->out.rows - n)));

  for (i = console->out.cols * (console->out.rows - n); i < console->out.cols * console->out.rows; i++) {
    console->out.buf[i].ch = ' ';
    console->out.buf[i].fg = COLOR_WHITE;
    console->out.buf[i].bg = COLOR_BLACK;
  }

  console->out.pos -= n * console->out.cols;

  if (console == console_current)
    mach_current->display_scroll_down(console, n);
}

static void
console_insert_rows(struct Tty *cons, unsigned rows)
{
  unsigned max_rows, start_pos, end_pos, n, i;
  
  max_rows  = cons->out.rows - cons->out.pos / cons->out.cols;
  rows      = MIN(max_rows, rows);

  start_pos = cons->out.pos - (cons->out.pos % cons->out.cols);
  end_pos   = start_pos + rows * cons->out.cols;
  n         = (max_rows - rows) * cons->out.cols;

  if (cons == console_current)
    mach_current->display_flush(cons);

  for (i = 0; i < rows * cons->out.cols; i++) {
    cons->out.buf[i + start_pos].ch = ' ';
    cons->out.buf[i + start_pos].fg = COLOR_WHITE;
    cons->out.buf[i + start_pos].bg = COLOR_BLACK;
  }

  memmove(&cons->out.buf[end_pos], &cons->out.buf[start_pos],
          sizeof(cons->out.buf[0]) * n);
  
  if (cons == console_current)
    mach_current->display_flush(cons);
}

static int
console_print_char(struct Tty *console, char c)
{ 
  int ret = 0;

  switch (c) {
  case '\n':
    if (console == console_current)
      mach_current->display_flush(console);

    console->out.pos += console->out.cols;
    console->out.pos -= console->out.pos % console->out.cols;

    if (console == console_current)
     mach_current->display_update_cursor(console);

    break;

  case '\r':
    if (console == console_current)
      mach_current->display_flush(console);

    console->out.pos -= console->out.pos % console->out.cols;

    if (console == console_current)
      mach_current->display_update_cursor(console);

    break;

  case '\b':
    if (console->out.pos > 0) {
      tty_flush(console);

      console->out.pos--;
      if (console == console_current)
        mach_current->display_update_cursor(console);
    }
    break;

  case '\t':
    do {
      console_set_char(console, console->out.pos++, ' ');
      ret++;
    } while ((console->out.pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  case C('G'):
    // TODO: beep;
    break;

  default:
    if (c < ' ') {
      console_set_char(console, console->out.pos++, '^');
      console_set_char(console, console->out.pos++, '@' + c);
      ret += 2;
    } else {
      console_set_char(console, console->out.pos++, c);
      ret++;
    }
    break;
  }

  if (console->out.pos >= console->out.cols * console->out.rows)
    console_scroll_down(console, 1);

  return ret;
}

void
console_dump(struct Tty *console, unsigned c)
{
  const char sym[] = "0123456789ABCDEF";

  console_print_char(console, '~');
  console_print_char(console, '~');
  console_print_char(console, '~');

  console_print_char(console, sym[(c >> 28) & 0xF]);
  console_print_char(console, sym[(c >> 24) & 0xF]);
  console_print_char(console, sym[(c >> 20) & 0xF]);
  console_print_char(console, sym[(c >> 16) & 0xF]);
  console_print_char(console, sym[(c >> 12) & 0xF]);
  console_print_char(console, sym[(c >> 8) & 0xF]);
  console_print_char(console, sym[(c >> 4) & 0xF]);
  console_print_char(console, sym[(c >> 0) & 0xF]);

  console_print_char(console, '\n');
}

// Handle the escape sequence terminated by the final character c.
static void
console_handle_esc(struct Tty *console, char c)
{
  int i, tmp;
  unsigned n, m;

  tty_flush(console);

  switch (c) {

  // Cursor Up
  case 'A':
    n = (console->out.esc_cur_param == 0) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.pos / console->out.cols);
    console->out.pos -= n * console->out.cols;
    break;
  
  // Cursor Down
  case 'B':
    n = (console->out.esc_cur_param == 0) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.rows - console->out.pos / console->out.cols - 1);
    console->out.pos += n * console->out.cols;
    break;

  // Cursor Forward
  case 'C':
    n = (console->out.esc_cur_param == 0) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.cols - console->out.pos % console->out.cols - 1);
    console->out.pos += n;
    break;

  // Cursor Back
  case 'D':
    n = (console->out.esc_cur_param == 0) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.pos % console->out.cols);
    console->out.pos -= n;
    break;

  // Cursor Horizontal Absolute
  case 'G':
    n = (console->out.esc_cur_param < 1) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.cols);
    console->out.pos -= console->out.pos % console->out.cols;
    console->out.pos += n - 1;
    break;

  // Cursor Position
  case 'H':
    n = (console->out.esc_cur_param < 1) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.rows);

    m = (console->out.esc_cur_param < 2) ? 1 : console->out.esc_params[1];
    m = MIN(m, console->out.cols);

    console->out.pos = (n - 1) * console->out.cols + m - 1;
    break;

  // Erase in Display
  case 'J':
    n = (console->out.esc_cur_param < 1) ? 0 : console->out.esc_params[0];
    if (n == 0) {
      console_erase(console, console->out.pos, CONSOLE_COLS * CONSOLE_ROWS - 1);
    } else if (n == 1) {
      console_erase(console, 0, console->out.pos);
    } else if (n == 2) {
      console_erase(console, 0, CONSOLE_COLS * CONSOLE_ROWS - 1);
    }
    break;

  // Erase in Line
  case 'K':
    n = (console->out.esc_cur_param < 1) ? 0 : console->out.esc_params[0];
    if (n == 0) {
      console_erase(console, console->out.pos, console->out.pos - console->out.pos % CONSOLE_COLS + CONSOLE_COLS - 1);
    } else if (n == 1) {
      console_erase(console, console->out.pos - console->out.pos % CONSOLE_COLS, console->out.pos);
    } else if (n == 2) {
      console_erase(console, console->out.pos - console->out.pos % CONSOLE_COLS, console->out.pos - console->out.pos % CONSOLE_COLS + CONSOLE_COLS - 1);
    }
    break;

  // Insert Line
  case 'L':
    n = (console->out.esc_cur_param < 1) ? 1 : console->out.esc_params[0];
    console_insert_rows(console, n);
    break;

  // Cursor Vertical Position
  case 'd':
    n = (console->out.esc_cur_param < 1) ? 1 : console->out.esc_params[0];
    n = MIN(n, console->out.rows);
    console->out.pos = (n - 1) * console->out.cols + console->out.pos % console->out.cols;
    break;

  case '@':
    n = (console->out.esc_cur_param < 1) ? 1 : console->out.esc_params[0];
    n = MIN(n, CONSOLE_COLS - console->out.pos % CONSOLE_COLS);

    for (m = console->out.pos - console->out.pos % CONSOLE_COLS + CONSOLE_COLS - 1; m >= console->out.pos; m--) {
      unsigned t = CONSOLE_COLS - console->out.pos % CONSOLE_COLS - n;
      if (m >= console->out.pos + 1) {
        console->out.buf[m] = console->out.buf[m - t];
      } else {
        console->out.buf[m].ch = ' ';
        console->out.buf[m].fg = console->out.fg_color;
        console->out.buf[m].bg = console->out.bg_color;
      }

      if (console == console_current)
        mach_current->display_draw_char_at(console, m);
    }
    break;
 
  // Set Graphic Rendition
  case 'm':
    if (console->out.esc_cur_param == 0) {
      // All attributes off
      console->out.bg_color = COLOR_BLACK;
      console->out.fg_color = COLOR_WHITE;
      break;
    }

    for (i = 0; (i < console->out.esc_cur_param) && (i < CONSOLE_ESC_MAX); i++) {
      switch (console->out.esc_params[i]) {
      case 0:   // Reset all modes (styles and colors)
        console->out.bg_color = COLOR_BLACK;
        console->out.fg_color = COLOR_WHITE;
        break;
      case 1:   // Set bold mode
        console->out.fg_color |= COLOR_BRIGHT;
        break;
      case 7:   // Set inverse/reverse mode
      case 27:
        tmp = console->out.bg_color;
        console->out.bg_color = console->out.fg_color;
        console->out.fg_color = tmp;
        break;
      case 22:  // Reset bold mode
        console->out.fg_color &= ~COLOR_BRIGHT;
        break;
      case 39:
        // Default foreground color (white)
        console->out.fg_color = (console->out.fg_color & ~COLOR_MASK) | COLOR_WHITE;
        break;
      case 49:
        // Default background color (black)
        console->out.bg_color = (console->out.bg_color & ~COLOR_MASK) | COLOR_BLACK;
        break;
      default:
        if ((console->out.esc_params[i] >= 30) && (console->out.esc_params[i] <= 37)) {
          // Set foreground color
          console->out.fg_color = (console->out.fg_color & ~COLOR_MASK) | (console->out.esc_params[i] - 30);
        } else if ((console->out.esc_params[i] >= 40) && (console->out.esc_params[i] <= 47)) {
          // Set background color
          console->out.bg_color = (console->out.bg_color & ~COLOR_MASK) | (console->out.esc_params[i] - 40);
        }
      }
    }
    break;

  case 'b':
    for (n = 0; n < console->out.esc_params[0]; n++)
      console_print_char(console, console->out.buf[console->out.pos - 1].ch);
    break;

  // TODO
  case 'n':
  case '%':
  case 'r':
  case 'h':
  case 'l':
    break;

  // TODO: handle other control sequences here

  default:
    console_dump(console, c);
    console_dump(console, console->out.esc_params[0]);
    for (;;);

    break;
  }

  tty_flush(console);
}

static void
tty_out_char(struct Tty *console, char c)
{
  int i;

  // The first (aka system) console is also connected to the serial port
  if (console == console_system)
    mach_current->serial_putc(c);

  switch (console->out.state) {
  case PARSER_NORMAL:
    if (c == '\x1b')
      console->out.state = PARSER_ESC;
    else
      console_print_char(console, c);
    break;

	case PARSER_ESC:
		if (c == '[') {
			console->out.state = PARSER_CSI;
      console->out.esc_cur_param = -1;
      console->out.esc_question = 0;
      for (i = 0; i < CONSOLE_ESC_MAX; i++)
        console->out.esc_params[i] = 0;
		} else {
      // console_dump(console, c);
			console->out.state = PARSER_NORMAL;
		}
		break;

	case PARSER_CSI:
    if (c == '?') {
      if (console->out.esc_cur_param == -1) {
        console->out.esc_question = 1;
      } else {
        console->out.state = PARSER_NORMAL;
      }
    } else {
      if (c >= '0' && c <= '9') {
        if (console->out.esc_cur_param == -1)
          console->out.esc_cur_param = 0;

        // Parse the current parameter
        if (console->out.esc_cur_param < CONSOLE_ESC_MAX)
          console->out.esc_params[console->out.esc_cur_param] = console->out.esc_params[console->out.esc_cur_param] * 10 + (c - '0');
      } else if (c == ';') {
        // Next parameter
        if (console->out.esc_cur_param < CONSOLE_ESC_MAX)
          console->out.esc_cur_param++;
      } else {
        if (console->out.esc_cur_param < CONSOLE_ESC_MAX)
          console->out.esc_cur_param++;

        console_handle_esc(console, c);
        console->out.state = PARSER_NORMAL;
      }
    }
		break;

	default:
		console->out.state = PARSER_NORMAL;
    break;
	}
}

static void
tty_echo(struct Tty *tty, char c)
{
  k_spinlock_acquire(&tty->out.lock);
  tty_out_char(tty, c);
  tty_flush(tty);
  k_spinlock_release(&tty->out.lock);
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  if (console_system != NULL)
    tty_echo(console_system, c);
}

static void
console_backspace(struct Tty *tty)
{
  k_spinlock_acquire(&tty->out.lock);
  if (tty->out.pos > 0)
    console_set_char(tty, --tty->out.pos, ' ');
  k_spinlock_release(&tty->out.lock);
}

static int
tty_erase_input(struct Tty *tty)
{
  if (tty->in.size == 0)
    return 0;

  if (tty->termios.c_lflag & ECHOE) {
    console_backspace(tty);
    tty_echo(tty, '\b');
  }

  tty->in.size--;
  if (tty->in.write_pos == 0) {
    tty->in.write_pos = CONSOLE_INPUT_MAX - 1;
  } else {
    tty->in.write_pos--;
  }
  
  return 1;
}


void
console_switch(int n)
{
  if (console_current != &consoles[n]) {
    console_current = &consoles[n];
    mach_current->display_update(console_current);
  }
}

/**
 * Return the next input character from the console. Polls for any pending
 * input characters.
 * 
 * @returns The next input character.
 */
int
console_getc(void)
{
  int c;
  
  // Poll for any pending characters from UART and the keyboard.
  while ((c = mach_current->console_getc()) <= 0)
    ;

  return c;
}

/**
 * Handle console interrupt.
 * 
 * This function should be called by driver interrupt routines to feed input
 * characters into the console buffer.
 * 
 * @param getc The function to fetch the next input character. Should return -1
 *             if there is no data avalable.
 */
void
tty_process_input(struct Tty *tty, char *buf)
{
  int c, status = 0;

  k_spinlock_acquire(&tty->in.lock);

  while ((c = *buf++) != 0) {
    if (c == '\0')
      break;

    // Strip character
    if (tty->termios.c_iflag & ISTRIP)
      c &= 0x7F;
    
    if (c == '\r') {
      // Ignore CR
      if (tty->termios.c_iflag & IGNCR)
        continue;
      // Map CR to NL
      if (tty->termios.c_iflag & ICRNL)
        c = '\n';
    } else if (c == '\n') {
      // Map NL to CR
      if (tty->termios.c_iflag & INLCR)
        c = '\r';
    }

    // Canonical input processing
    if (tty->termios.c_lflag & ICANON) {
      // ERASE character
      if (c == tty->termios.c_cc[VERASE]) {
        tty_erase_input(tty);
        continue;
      }

      // KILL character
      if (c == tty->termios.c_cc[VKILL]) {
        while (tty_erase_input(tty))
          ;

        if (tty->termios.c_lflag & ECHOK)
          tty_echo(tty, c);

        continue;
      }

      // EOF character
      if (c == tty->termios.c_cc[VEOF])
        status |= IN_EOF;

      // EOL character
      if ((c == tty->termios.c_cc[VEOL]) || (c == '\n'))
        status |= IN_EOL;
    }

    // Handle flow control characters
    if (tty->termios.c_iflag & (IXON | IXOFF)) {
      if (c == tty->termios.c_cc[VSTOP]) {
        k_spinlock_acquire(&tty->out.lock);
        tty->out.stopped = 1;
        k_spinlock_release(&tty->out.lock);

        if (tty->termios.c_iflag & IXOFF)
          tty_echo(tty, c);

        continue;
      }

      if ((c == tty->termios.c_cc[VSTART]) || (tty->termios.c_iflag & IXANY)) {
        k_spinlock_acquire(&tty->out.lock);
        tty->out.stopped = 0;
        k_spinlock_release(&tty->out.lock);

        if (c == tty->termios.c_cc[VSTART]) {
          if (tty->termios.c_iflag & IXOFF)
            tty_echo(tty, c);
          continue;
        }
      }
    }

    // Recognize signals
    if (tty->termios.c_lflag & ISIG) {
      int sig;

      if (c == tty->termios.c_cc[VINTR])
        sig = SIGINT;
      else if (c == tty->termios.c_cc[VQUIT])
        sig = SIGQUIT;
      else if (c == tty->termios.c_cc[VSUSP])
        sig = SIGSTOP;
      else
        sig = 0;

      if (sig) {
        tty_signal(tty, sig);
        tty_echo(tty, c);
        continue;
      }
    }

    if ((c != tty->termios.c_cc[VEOF]) && (tty->termios.c_lflag & ECHO))
      tty_echo(tty, c);
    else if ((c == '\n') && (tty->termios.c_lflag & ECHONL))
      tty_echo(tty, c);

    if (tty->in.size == (CONSOLE_INPUT_MAX - 1)) {
      // Reserve space for one EOL character at the end of the input buffer
      if (!(tty->termios.c_lflag & ICANON))
        continue;
      if ((c != tty->termios.c_cc[VEOL]) &&
          (c != tty->termios.c_cc[VEOF]) &&
          (c != '\n'))
        continue;
    } else if (tty->in.size == CONSOLE_INPUT_MAX) {
      // Input buffer full - discard all extra characters
      continue;
    }

    tty->in.buf[tty->in.write_pos] = c;
    tty->in.write_pos = (tty->in.write_pos + 1) % CONSOLE_INPUT_MAX;
    tty->in.size++;
  }

  if ((status & (IN_EOF | IN_EOL)) || !(tty->termios.c_lflag & ICANON))
    k_waitqueue_wakeup_all(&tty->in.queue);

  k_spinlock_release(&tty->in.lock);
}

/**
 * Read from the console.
 * 
 * @param inode  The inode corresponding to the console
 * @param buf    User virtual address where to store the data
 * @param nbytes The number of bytes to read
 * 
 * @return The number of bytes read or a negative value if an error occured.
 */
ssize_t
tty_read(dev_t dev, uintptr_t buf, size_t nbytes)
{
  struct Tty *tty = tty_from_dev(dev);
  size_t i = 0;

  if (tty == NULL)
    return -ENODEV;

  k_spinlock_acquire(&tty->in.lock);

  while (i < nbytes) {
    char c;
    int r;

    // Wait for input
    while (tty->in.size == 0) {
      if ((r = k_waitqueue_sleep(&tty->in.queue, &tty->in.lock)) < 0) {
        k_spinlock_release(&tty->in.lock);
        return r;
      }
    }

    // Grab the next character
    c = tty->in.buf[tty->in.read_pos];
    tty->in.read_pos = (tty->in.read_pos + 1) % CONSOLE_INPUT_MAX;
    tty->in.size--;

    if (tty->termios.c_lflag & ICANON) {
      // EOF is only recognized in canonical mode
      if (c == tty->termios.c_cc[VEOF])
        break;

      if ((r = vm_space_copy_out(&c, buf++, 1)) < 0) {
        k_spinlock_release(&tty->in.lock);
        return r;
      }
      i++;

      // In canonical mode, we process at most a single line of input
      if ((c == tty->termios.c_cc[VEOL]) || (c == '\n'))
        break;
    } else {
      if ((r = vm_space_copy_out(&c, buf++, 1)) < 0) {
        k_spinlock_release(&tty->in.lock);
        return r;
      }
      i++;

      if (i >= tty->termios.c_cc[VMIN])
        break;
    }
  }

  k_spinlock_release(&tty->in.lock);

  return i;
}

ssize_t
tty_write(dev_t dev, uintptr_t buf, size_t nbytes)
{
  struct Tty *tty = tty_from_dev(dev);
  size_t i;

  if (tty == NULL)
    return -ENODEV;

  k_spinlock_acquire(&tty->out.lock);

  if (!tty->out.stopped) {
    // TODO: unlock periodically to not block the entire system?
    for (i = 0; i != nbytes; i++) {
      int c, r;

      if ((r = vm_space_copy_in(&c, buf + i, 1)) < 0) {
        k_spinlock_release(&tty->out.lock);
        return r;
      }

      tty_out_char(tty, c);
    }

    tty_flush(tty);
  }

  k_spinlock_release(&tty->out.lock);
  
  return i;
}

int
tty_ioctl(dev_t dev, int request, int arg)
{
  struct Tty *tty = tty_from_dev(dev);
  struct winsize ws;

  if (tty == NULL)
    return -ENODEV;

  switch (request) {
  case TIOCGETA:
    return vm_copy_out(process_current()->vm->pgtab,
                       &tty->termios,
                       arg,
                       sizeof(struct termios));

  case TIOCSETAW:
    // TODO: drain
  case TIOCSETA:
    return vm_copy_in(process_current()->vm->pgtab,
                      &tty->termios,
                      arg,
                      sizeof(struct termios));

  case TIOCGPGRP:
    return console_current->pgrp;
  case TIOCSPGRP:
    // TODO: validate
    console_current->pgrp = arg;
    return 0;
  case TIOCGWINSZ:
    ws.ws_col = CONSOLE_COLS;
    ws.ws_row = CONSOLE_ROWS;
    ws.ws_xpixel = DEFAULT_FB_WIDTH;
    ws.ws_ypixel = DEFAULT_FB_HEIGHT;
    return vm_copy_out(process_current()->vm->pgtab, &ws, arg, sizeof ws);
  case TIOCSWINSZ:
    // cprintf("set winsize\n");
    if (vm_copy_in(process_current()->vm->pgtab, &ws, arg, sizeof ws) < 0)
      return -EFAULT;
    // cprintf("ws_col = %d\n", ws.ws_col);
    // cprintf("ws_row = %d\n", ws.ws_row);
    // cprintf("ws_xpixel = %d\n", ws.ws_xpixel);
    // cprintf("ws_ypixel = %d\n", ws.ws_ypixel);
    return 0;
  default:
    panic("TODO: %p - %d %c %d\n", request, request & 0xFF, (request >> 8) & 0xF, (request >> 16) & 0x1FFF);
    return -EINVAL;
  }
}

static int
tty_try_select(struct Tty *tty)
{
  if (tty->in.size > 0) {
    if (!(tty->termios.c_lflag & ICANON)) {
      return 1;
    } else {
      // TODO: check whether EOL or EOF seen
      return 0;
    }
  }
  return 0;
}

int
tty_select(dev_t dev, struct timeval *timeout)
{
  struct Tty *tty = tty_from_dev(dev);
  int r;

  if (tty == NULL)
    return -ENODEV;

  k_spinlock_acquire(&tty->in.lock);

  while ((r = tty_try_select(tty)) == 0) {
    unsigned long t = 0;

    if (timeout != NULL) {
      t = timeout->tv_sec * TICKS_PER_SECOND + timeout->tv_usec / US_PER_TICK;
      k_spinlock_release(&tty->in.lock);
      return 0;
    }

    if ((r = k_waitqueue_timed_sleep(&tty->in.queue, &tty->in.lock, t)) < 0) {
      k_spinlock_release(&tty->in.lock);
      return r;
    }
  }

  k_spinlock_release(&tty->in.lock);

  return r;
}
