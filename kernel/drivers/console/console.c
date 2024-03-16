#include <errno.h>
#include <stdint.h>
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

#include "kbd.h"
#include "display.h"
#include "serial.h"

#define NCONSOLES 6

struct Console consoles[NCONSOLES];
struct Console *console_current = &consoles[0];

#define IN_EOF  (1 << 0)
#define IN_EOL  (1 << 1)

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  struct Console *console;

  for (console = consoles; console < &consoles[NCONSOLES]; console++) {
    unsigned i;

    spin_init(&console->in.lock, "console.in");
    wchan_init(&console->in.queue);

    console->out.fg_color      = COLOR_WHITE;
    console->out.bg_color      = COLOR_BLACK;
    console->out.state         = PARSER_NORMAL;
    console->out.esc_cur_param = -1;
    console->out.cols          = CONSOLE_COLS;
    console->out.rows          = CONSOLE_ROWS;
    spin_init(&console->out.lock, "console.out");

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
  }

  serial_init();
  kbd_init();
  display_init();

  display_update(console_current);
}

static void
console_out_flush(struct Console *console)
{
  if (console == console_current) {
    display_flush(console);
    display_update_cursor(console);
  }
}

static struct Console *
console_get(struct Inode *inode)
{
  int minor = inode->rdev & 0xFF;
  return minor < NCONSOLES ? &consoles[minor] : NULL;
}


/*
 * ----------------------------------------------------------------------------
 * Simple parser for ANSI escape sequences
 * ----------------------------------------------------------------------------
 * 
 * The following escape sequences are currently supported:
 *   CSI n m - Select Graphic Rendition
 *
 */

static void
console_erase(struct Console *console, unsigned from, unsigned to)
{
  size_t i;

  for (i = from; i <= to; i++) {
    console->out.buf[i].ch = ' ';
    console->out.buf[i].fg = console->out.fg_color;
    console->out.buf[i].bg = console->out.bg_color;
  }

  if (console == console_current)
    display_erase(console, from, to);
}

static void
console_set_char(struct Console *console, unsigned i, char c)
{
  console->out.buf[i].ch = c;
  console->out.buf[i].fg = console->out.fg_color & 0xF;
  console->out.buf[i].bg = console->out.bg_color & 0xF;
}

#define DISPLAY_TAB_WIDTH  4

static void
console_scroll_down(struct Console *console, unsigned n)
{
  unsigned i;

  if (console == console_current)
    display_flush(console);

  memmove(&console->out.buf[0], &console->out.buf[console->out.cols * n],
          sizeof(console->out.buf[0]) * (console->out.cols * (console->out.rows - n)));

  for (i = console->out.cols * (console->out.rows - n); i < console->out.cols * console->out.rows; i++) {
    console->out.buf[i].ch = ' ';
    console->out.buf[i].fg = COLOR_WHITE;
    console->out.buf[i].bg = COLOR_BLACK;
  }

  console->out.pos -= n * console->out.cols;

  if (console == console_current)
    display_scroll_down(console, n);
}

static int
console_print_char(struct Console *console, char c)
{ 
  int ret = 0;

  switch (c) {
  case '\n':
    if (console == console_current)
      display_flush(console);

    console->out.pos += console->out.cols;
    console->out.pos -= console->out.pos % console->out.cols;

    if (console == console_current)
      display_update_cursor(console);

    break;

  case '\r':
    if (console == console_current)
      display_flush(console);

    console->out.pos -= console->out.pos % console->out.cols;

    if (console == console_current)
      display_update_cursor(console);

    break;

  case '\b':
    if (console->out.pos > 0)
      console_set_char(console, --console->out.pos, ' ');
    break;

  case '\t':
    do {
      console_set_char(console, console->out.pos++, ' ');
      ret++;
    } while ((console->out.pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  default:
    console_set_char(console, console->out.pos++, c);
    ret++;
    break;
  }

  if (console->out.pos >= console->out.cols * console->out.rows)
    console_scroll_down(console, 1);

  return ret;
}

void
console_dump(struct Console *console, unsigned c)
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
console_handle_esc(struct Console *console, char c)
{
  int i, tmp;
  unsigned n, m;

  console_out_flush(console);

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
    for (;;);
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
    m = (console->out.esc_cur_param < 2) ? 1 : console->out.esc_params[1];
    n = MIN(n, console->out.rows);
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
        display_draw_char_at(console, m);
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

  // TODO: handle other control sequences here

  default:
    console_dump(console, c);
    console_dump(console, console->out.esc_params[0]);
    for (;;);

    break;
  }

  console_out_flush(console);
}

static void
console_out_char(struct Console *console, char c)
{
  int i;

  if (console == console_current)
    serial_putc(c);

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
console_echo(struct Console *console, char c)
{
  spin_lock(&console->out.lock);
  console_out_char(console, c);
  console_out_flush(console);
  spin_unlock(&console->out.lock);
}


/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  console_echo(console_current, c);
}

ssize_t
console_write(struct Inode *inode, const void *buf, size_t nbytes)
{
  struct Console *console = console_get(inode);
  const char *s = (const char *) buf;
  size_t i;

  if (console == NULL)
    return -EBADF;

  spin_lock(&console->out.lock);

  if (!console->out.stopped) {
    // TODO: unlock periodically to not block the entire system?
    for (i = 0; i != nbytes; i++)
      console_out_char(console, s[i]);
    console_out_flush(console);
  }

  spin_unlock(&console->out.lock);
  
  return i;
}

static int pgrp;

int
console_ioctl(struct Inode *inode, int request, int arg)
{
  struct Console *console = console_get(inode);
  struct winsize ws;

  if (console == NULL)
    return -EBADF;

  switch (request) {
  case TIOCGETA:
    return vm_copy_out(process_current()->vm->pgtab,
                       &console->termios,
                       arg,
                       sizeof(struct termios));

  case TIOCSETAW:
    // TODO: drain
  case TIOCSETA:
    return vm_copy_in(process_current()->vm->pgtab,
                      &console->termios,
                      arg,
                      sizeof(struct termios));

  case TIOCGPGRP:
    return pgrp;
  case TIOCSPGRP:
    // TODO: validate
    pgrp = arg;
    return 0;
  case TIOCGWINSZ:
    ws.ws_col = CONSOLE_COLS;
    ws.ws_row = CONSOLE_ROWS;
    ws.ws_xpixel = DEFAULT_FB_WIDTH;
    ws.ws_ypixel = DEFAULT_FB_HEIGHT;
    return vm_copy_out(process_current()->vm->pgtab, &ws, arg, sizeof ws);

  default:
    panic("TODO: %d\n", request & 0xFF);
    return -EINVAL;
  }
}

static int
console_back(struct Console *cons)
{
  if (cons->in.size == 0)
    return 0;

  if (cons->termios.c_lflag & ECHOE)
    console_echo(cons, '\b');

  cons->in.size--;
  if (cons->in.write_pos == 0) {
    cons->in.write_pos = CONSOLE_INPUT_MAX - 1;
  } else {
    cons->in.write_pos--;
  }
  
  return 1;
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
console_interrupt(char *buf)
{
  struct Console *cons = console_current;
  int c, status = 0;

  spin_lock(&cons->in.lock);

  while ((c = *buf++) != 0) {
    if (c == '\0')
      break;

    // Strip character
    if (cons->termios.c_iflag & ISTRIP)
      c &= 0x7F;
    
    if (c == '\r') {
      // Ignore CR
      if (cons->termios.c_iflag & IGNCR)
        continue;
      // Map CR to NL
      if (cons->termios.c_iflag & ICRNL)
        c = '\n';
    } else if (c == '\n') {
      // Map NL to CR
      if (cons->termios.c_iflag & INLCR)
        c = '\r';
    }

    // Canonical input processing
    if (cons->termios.c_lflag & ICANON) {
      // ERASE character
      if (c == cons->termios.c_cc[VERASE]) {
        console_back(cons);
        continue;
      }

      // KILL character
      if (c == cons->termios.c_cc[VKILL]) {
        while (console_back(cons))
          ;

        if (cons->termios.c_lflag & ECHOK)
          console_echo(cons, c);

        continue;
      }

      // EOF character
      if (c == cons->termios.c_cc[VEOF])
        status |= IN_EOF;

      // EOL character
      if ((c == cons->termios.c_cc[VEOL]) || (c == '\n'))
        status |= IN_EOL;
    }

    // Handle flow control characters
    if (cons->termios.c_iflag & (IXON | IXOFF)) {
      if (c == cons->termios.c_cc[VSTOP]) {
        spin_lock(&cons->out.lock);
        cons->out.stopped = 1;
        spin_unlock(&cons->out.lock);

        if (cons->termios.c_iflag & IXOFF)
          console_echo(cons, c);

        continue;
      }

      if (c == cons->termios.c_cc[VSTART] || cons->termios.c_iflag & IXANY) {
        spin_lock(&cons->out.lock);
        cons->out.stopped = 0;
        spin_unlock(&cons->out.lock);

        if (c == cons->termios.c_cc[VSTART]) {
          if (cons->termios.c_iflag & IXOFF)
            console_echo(cons, c);
          continue;
        }
      }
    }

    if ((c != cons->termios.c_cc[VEOF]) && (cons->termios.c_lflag & ECHO))
      console_echo(cons, c);
    else if ((c == '\n') && (cons->termios.c_lflag & ECHONL))
      console_echo(cons, c);

    if (cons->in.size == (CONSOLE_INPUT_MAX - 1)) {
      // Reserve space for one EOL character at the end of the input buffer
      if (!(cons->termios.c_lflag & ICANON))
        continue;
      if ((c != cons->termios.c_cc[VEOL]) &&
          (c != cons->termios.c_cc[VEOF]) &&
          (c != '\n'))
        continue;
    } else if (cons->in.size == CONSOLE_INPUT_MAX) {
      // Input buffer full - discard all extra characters
      continue;
    }

    cons->in.buf[cons->in.write_pos] = c;
    cons->in.write_pos = (cons->in.write_pos + 1) % CONSOLE_INPUT_MAX;
    cons->in.size++;
  }

  if ((status & (IN_EOF | IN_EOL)) || !(cons->termios.c_lflag & ICANON))
    wchan_wakeup_all(&cons->in.queue);

  spin_unlock(&cons->in.lock);
}

void
console_switch(int n)
{
  if (console_current != &consoles[n]) {
    console_current = &consoles[n];
    display_update(console_current);
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
  while (((c = kbd_getc()) <= 0) && ((c = serial_getc()) <= 0))
    ;

  return c;
}

ssize_t
console_read(struct Inode *inode, uintptr_t buf, size_t nbytes)
{
  struct Console *cons = console_get(inode);
  char *s = (char *) buf;
  size_t i = 0;
  
  if (cons == NULL)
    return -EBADF;

  spin_lock(&cons->in.lock);

  while (i < nbytes) {
    char c;

    while (cons->in.size == 0)
      wchan_sleep(&cons->in.queue, &cons->in.lock);

    c = cons->in.buf[cons->in.read_pos];
    cons->in.read_pos = (cons->in.read_pos + 1) % CONSOLE_INPUT_MAX;
    cons->in.size--;

    if (cons->termios.c_lflag & ICANON) {
      if (c == cons->termios.c_cc[VEOF])
        break;

      s[i++] = c;

      if ((c == cons->termios.c_cc[VEOL]) || (c == '\n'))
        break;
    } else {
      s[i++] = c;
    }
  }

  spin_unlock(&cons->in.lock);

  return i;
}
