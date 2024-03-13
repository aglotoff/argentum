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

#include "kbd.h"
#include "display.h"
#include "serial.h"

#define NCONSOLES 6

struct Console consoles[NCONSOLES];
struct Console *console_current = &consoles[0];

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  struct Console *console;

  for (console = consoles; console < &consoles[NCONSOLES]; console++) {
    unsigned i;

    spin_init(&console->in.lock, "input");
    wchan_init(&console->in.queue);

    console->termios.c_lflag = ICANON | ECHO;

    console->out.fg_color      = COLOR_WHITE;
    console->out.bg_color      = COLOR_BLACK;
    console->out.state         = PARSER_NORMAL;
    console->out.esc_cur_param = -1;
    console->out.cols          = CONSOLE_COLS;
    console->out.rows          = CONSOLE_ROWS;

    for (i = 0; i < console->out.cols * console->out.rows; i++) {
      console->out.buf[i].ch = ' ';
      console->out.buf[i].fg = COLOR_WHITE;
      console->out.buf[i].bg = COLOR_BLACK;
    }
  }

  serial_init();
  kbd_init();
  display_init();

  display_update(console_current);
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
  struct Console *console = console_current;

  int c;

  spin_lock(&console->in.lock);

  while ((c = *buf++) != 0) {
    switch (c) {
    case '\0':
      break;
    case '\b':
      if (console->in.size > 0) {
        console_putc(c);

        console->in.size--;
        if (console->in.write_pos == 0) {
          console->in.write_pos = CONSOLE_INPUT_MAX - 1;
        } else {
          console->in.write_pos--;
        }
      }
      break;
    default:
      if (c != C('D') && (console->termios.c_lflag & ECHO))
        console_putc(c);

      console->in.buf[console->in.write_pos] = c;
      console->in.write_pos = (console->in.write_pos + 1) % CONSOLE_INPUT_MAX;

      if (console->in.size == CONSOLE_INPUT_MAX) {
        console->in.read_pos = (console->in.read_pos + 1) % CONSOLE_INPUT_MAX;
      } else {
        console->in.size++;
      }

      if ((c == '\r') || (c == '\n') || (c == C('D')) ||
          !(console->termios.c_lflag & ICANON) ||
          (console->in.size == CONSOLE_INPUT_MAX))
        wchan_wakeup_all(&console->in.queue);
      break;
    }
  }

  spin_unlock(&console->in.lock);
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

static struct Console *
console_get(struct Inode *inode)
{
  int minor = inode->rdev & 0xFF;
  return minor < NCONSOLES ? &consoles[minor] : NULL;
}

ssize_t
console_read(struct Inode *inode, uintptr_t buf, size_t nbytes)
{
  struct Console *console = console_get(inode);
  char c, *s;
  size_t i;
  
  if (console == NULL)
    return -EBADF;
  
  s = (char *) buf;
  i = 0;

  spin_lock(&console->in.lock);

  while (i < nbytes) {
    while (console->in.size == 0)
      wchan_sleep(&console->in.queue, &console->in.lock);

    c = console->in.buf[console->in.read_pos];
    console->in.read_pos = (console->in.read_pos + 1) % CONSOLE_INPUT_MAX;
    console->in.size--;

    // End of input
    if (c == C('D'))
      break;

    // Stop if a newline is encountered
    if ((s[i++] = c) == '\n')
      break;
  }

  spin_unlock(&console->in.lock);
  
  return i;
}

static void
console_out_flush(struct Console *console)
{
  if (console == console_current) {
    display_flush(console);
    display_update_cursor(console);
  }
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
  size_t n = to - from + 1;

  memset(&console->out.buf[from], 0, (sizeof console->out.buf[0]) * n);

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

static void
console_print_char(struct Console *console, char c)
{ 
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
    } while ((console->out.pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  default:
    console_set_char(console, console->out.pos++, c);
    break;
  }

  if (console->out.pos >= console->out.cols * console->out.rows)
    console_scroll_down(console, 1);
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

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  console_out_char(console_current, c);
  console_out_flush(console_current);
}

ssize_t
console_write(struct Inode *inode, const void *buf, size_t nbytes)
{
  struct Console *console = console_get(inode);
  const char *s = (const char *) buf;
  size_t i;

  if (console == NULL)
    return -EBADF;

  for (i = 0; i != nbytes; i++)
    console_out_char(console, s[i]);

  console_out_flush(console);
  
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
