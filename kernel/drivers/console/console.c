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

#define NTTYS       6   // The total number of virtual ttys
#define NSCREENS    6   // For now, all ttys are screens

static struct Screen screens[NSCREENS];

static struct Tty ttys[NTTYS];
struct Tty *tty_current;
struct Tty *tty_system;

#define IN_EOF  (1 << 0)
#define IN_EOL  (1 << 1)

static void
screen_init(struct Screen *screen)
{
  unsigned i;

  screen->fg_color      = COLOR_WHITE;
  screen->bg_color      = COLOR_BLACK;
  screen->state         = PARSER_NORMAL;
  screen->esc_cur_param = -1;
  screen->cols          = SCREEN_COLS;
  screen->rows          = SCREEN_ROWS;
  k_spinlock_init(&screen->lock, "screen");

  for (i = 0; i < screen->cols * screen->rows; i++) {
    screen->buf[i].ch = ' ';
    screen->buf[i].fg = COLOR_WHITE;
    screen->buf[i].bg = COLOR_BLACK;
  }
}

/**
 * Initialize the console devices.
 */
void
tty_init(void)
{  
  int i;

  for (i = 0; i < NTTYS; i++) {
    struct Tty *tty = &ttys[i];

    k_spinlock_init(&tty->in.lock, "tty.in");
    k_waitqueue_init(&tty->in.queue);

    tty->out = &screens[i];
    screen_init(tty->out);

    tty->termios.c_iflag = BRKINT | ICRNL | IXON | IXANY;
    tty->termios.c_oflag = OPOST | ONLCR;
    tty->termios.c_cflag = CREAD | CS8 | HUPCL;
    tty->termios.c_lflag = ISIG | ICANON | ECHO | ECHOE;
    tty->termios.c_cc[VEOF]   = C('D');
    tty->termios.c_cc[VEOL]   = _POSIX_VDISABLE;
    tty->termios.c_cc[VERASE] = C('H');
    tty->termios.c_cc[VINTR]  = C('C');
    tty->termios.c_cc[VKILL]  = C('U');
    tty->termios.c_cc[VMIN]   = 1;
    tty->termios.c_cc[VQUIT]  = C('\\');
    tty->termios.c_cc[VTIME]  = 0;
    tty->termios.c_cc[VSUSP]  = C('Z');
    tty->termios.c_cc[VSTART] = C('Q');
    tty->termios.c_cc[VSTOP]  = C('S');
    tty->termios.c_ispeed = B9600;
    tty->termios.c_ospeed = B9600;
  }

  tty_current = &ttys[0];
  tty_system  = &ttys[0];

  mach_current->console_init();
  mach_current->display_update(tty_current->out);

  dev_register_char(0x01, &tty_device);
}


int
screen_is_current(struct Screen *screen)
{
  return (tty_current != NULL) && (screen == tty_current->out);
}

static void
screen_flush(struct Screen *screen)
{
  if (screen_is_current(screen)) {
    mach_current->display_flush(screen);
    mach_current->display_update_cursor(screen);
  }
}

static void
screen_erase(struct Screen *screen, unsigned from, unsigned to)
{
  size_t i;

  for (i = from; i <= to; i++) {
    screen->buf[i].ch = ' ';
    screen->buf[i].fg = screen->fg_color;
    screen->buf[i].bg = screen->bg_color;
  }

  if (screen_is_current(screen))
    mach_current->display_erase(screen, from, to);
}

static void
screen_set_char(struct Screen *screen, unsigned i, char c)
{
  screen->buf[i].ch = c;
  screen->buf[i].fg = screen->fg_color & 0xF;
  screen->buf[i].bg = screen->bg_color & 0xF;
}

#define DISPLAY_TAB_WIDTH  4

static void
screen_scroll_down(struct Screen *screen, unsigned n)
{
  unsigned i;

  if (screen_is_current(screen))
    mach_current->display_flush(screen);

  memmove(&screen->buf[0], &screen->buf[screen->cols * n],
          sizeof(screen->buf[0]) * (screen->cols * (screen->rows - n)));

  for (i = screen->cols * (screen->rows - n); i < screen->cols * screen->rows; i++) {
    screen->buf[i].ch = ' ';
    screen->buf[i].fg = COLOR_WHITE;
    screen->buf[i].bg = COLOR_BLACK;
  }

  screen->pos -= n * screen->cols;

  if (screen_is_current(screen))
    mach_current->display_scroll_down(screen, n);
}

static void
screen_insert_rows(struct Screen *screen, unsigned rows)
{
  unsigned max_rows, start_pos, end_pos, n, i;
  
  max_rows  = screen->rows - screen->pos / screen->cols;
  rows      = MIN(max_rows, rows);

  start_pos = screen->pos - (screen->pos % screen->cols);
  end_pos   = start_pos + rows * screen->cols;
  n         = (max_rows - rows) * screen->cols;

  if (screen_is_current(screen))
    mach_current->display_flush(screen);

  for (i = 0; i < rows * screen->cols; i++) {
    screen->buf[i + start_pos].ch = ' ';
    screen->buf[i + start_pos].fg = COLOR_WHITE;
    screen->buf[i + start_pos].bg = COLOR_BLACK;
  }

  memmove(&screen->buf[end_pos], &screen->buf[start_pos],
          sizeof(screen->buf[0]) * n);
  
  if (screen_is_current(screen))
    mach_current->display_flush(screen);
}

static int
screen_print_char(struct Screen *screen, char c)
{ 
  int ret = 0;

  switch (c) {
  case '\n':
    if (screen_is_current(screen))
      mach_current->display_flush(screen);

    screen->pos += screen->cols;
    screen->pos -= screen->pos % screen->cols;

    if (screen_is_current(screen))
     mach_current->display_update_cursor(screen);

    break;

  case '\r':
    if (screen_is_current(screen))
      mach_current->display_flush(screen);

    screen->pos -= screen->pos % screen->cols;

    if (screen_is_current(screen))
      mach_current->display_update_cursor(screen);

    break;

  case '\b':
    if (screen->pos > 0) {
      screen_flush(screen);

      screen->pos--;
      if (screen_is_current(screen))
        mach_current->display_update_cursor(screen);
    }
    break;

  case '\t':
    do {
      screen_set_char(screen, screen->pos++, ' ');
      ret++;
    } while ((screen->pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  case C('G'):
    // TODO: beep;
    break;

  default:
    if (c < ' ') {
      screen_set_char(screen, screen->pos++, '^');
      screen_set_char(screen, screen->pos++, '@' + c);
      ret += 2;
    } else {
      screen_set_char(screen, screen->pos++, c);
      ret++;
    }
    break;
  }

  if (screen->pos >= screen->cols * screen->rows)
    screen_scroll_down(screen, 1);

  return ret;
}

void
screen_dump(struct Screen *screen, unsigned c)
{
  const char sym[] = "0123456789ABCDEF";

  screen_print_char(screen, '~');
  screen_print_char(screen, '~');
  screen_print_char(screen, '~');

  screen_print_char(screen, sym[(c >> 28) & 0xF]);
  screen_print_char(screen, sym[(c >> 24) & 0xF]);
  screen_print_char(screen, sym[(c >> 20) & 0xF]);
  screen_print_char(screen, sym[(c >> 16) & 0xF]);
  screen_print_char(screen, sym[(c >> 12) & 0xF]);
  screen_print_char(screen, sym[(c >> 8) & 0xF]);
  screen_print_char(screen, sym[(c >> 4) & 0xF]);
  screen_print_char(screen, sym[(c >> 0) & 0xF]);

  screen_print_char(screen, '\n');
}

// Handle the escape sequence terminated by the final character c.
static void
screen_handle_esc(struct Screen *screen, char c)
{
  int i, tmp;
  unsigned n, m;

  screen_flush(screen);

  switch (c) {

  // Cursor Up
  case 'A':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->pos / screen->cols);
    screen->pos -= n * screen->cols;
    break;
  
  // Cursor Down
  case 'B':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->rows - screen->pos / screen->cols - 1);
    screen->pos += n * screen->cols;
    break;

  // Cursor Forward
  case 'C':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->cols - screen->pos % screen->cols - 1);
    screen->pos += n;
    break;

  // Cursor Back
  case 'D':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->pos % screen->cols);
    screen->pos -= n;
    break;

  // Cursor Horizontal Absolute
  case 'G':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->cols);
    screen->pos -= screen->pos % screen->cols;
    screen->pos += n - 1;
    break;

  // Cursor Position
  case 'H':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->rows);

    m = (screen->esc_cur_param < 2) ? 1 : screen->esc_params[1];
    m = MIN(m, screen->cols);

    screen->pos = (n - 1) * screen->cols + m - 1;
    break;

  // Erase in Display
  case 'J':
    n = (screen->esc_cur_param < 1) ? 0 : screen->esc_params[0];
    if (n == 0) {
      screen_erase(screen, screen->pos, SCREEN_COLS * SCREEN_ROWS - 1);
    } else if (n == 1) {
      screen_erase(screen, 0, screen->pos);
    } else if (n == 2) {
      screen_erase(screen, 0, SCREEN_COLS * SCREEN_ROWS - 1);
    }
    break;

  // Erase in Line
  case 'K':
    n = (screen->esc_cur_param < 1) ? 0 : screen->esc_params[0];
    if (n == 0) {
      screen_erase(screen, screen->pos, screen->pos - screen->pos % SCREEN_COLS + SCREEN_COLS - 1);
    } else if (n == 1) {
      screen_erase(screen, screen->pos - screen->pos % SCREEN_COLS, screen->pos);
    } else if (n == 2) {
      screen_erase(screen, screen->pos - screen->pos % SCREEN_COLS, screen->pos - screen->pos % SCREEN_COLS + SCREEN_COLS - 1);
    }
    break;

  // Insert Line
  case 'L':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    screen_insert_rows(screen, n);
    break;

  // Cursor Vertical Position
  case 'd':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->rows);
    screen->pos = (n - 1) * screen->cols + screen->pos % screen->cols;
    break;

  case '@':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, SCREEN_COLS - screen->pos % SCREEN_COLS);

    for (m = screen->pos - screen->pos % SCREEN_COLS + SCREEN_COLS - 1; m >= screen->pos; m--) {
      if (m > screen->pos) {
        screen->buf[m] = screen->buf[m - n];
      } else {
        screen->buf[m].ch = ' ';
        screen->buf[m].fg = screen->fg_color;
        screen->buf[m].bg = screen->bg_color;
      }

      if (screen_is_current(screen))
        mach_current->display_draw_char_at(screen, m);
    }
    break;
 
  // Set Graphic Rendition
  case 'm':
    if (screen->esc_cur_param == 0) {
      // All attributes off
      screen->bg_color = COLOR_BLACK;
      screen->fg_color = COLOR_WHITE;
      break;
    }

    for (i = 0; (i < screen->esc_cur_param) && (i < SCREEN_ESC_MAX); i++) {
      switch (screen->esc_params[i]) {
      case 0:   // Reset all modes (styles and colors)
        screen->bg_color = COLOR_BLACK;
        screen->fg_color = COLOR_WHITE;
        break;
      case 1:   // Set bold mode
        screen->fg_color |= COLOR_BRIGHT;
        break;
      case 7:   // Set inverse/reverse mode
      case 27:
        tmp = screen->bg_color;
        screen->bg_color = screen->fg_color;
        screen->fg_color = tmp;
        break;
      case 22:  // Reset bold mode
        screen->fg_color &= ~COLOR_BRIGHT;
        break;
      case 39:
        // Default foreground color (white)
        screen->fg_color = (screen->fg_color & ~COLOR_MASK) | COLOR_WHITE;
        break;
      case 49:
        // Default background color (black)
        screen->bg_color = (screen->bg_color & ~COLOR_MASK) | COLOR_BLACK;
        break;
      default:
        if ((screen->esc_params[i] >= 30) && (screen->esc_params[i] <= 37)) {
          // Set foreground color
          screen->fg_color = (screen->fg_color & ~COLOR_MASK) | (screen->esc_params[i] - 30);
        } else if ((screen->esc_params[i] >= 40) && (screen->esc_params[i] <= 47)) {
          // Set background color
          screen->bg_color = (screen->bg_color & ~COLOR_MASK) | (screen->esc_params[i] - 40);
        }
      }
    }
    break;

  case 'b':
    for (n = 0; n < screen->esc_params[0]; n++)
      screen_print_char(screen, screen->buf[screen->pos - 1].ch);
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
    screen_dump(screen, c);
    screen_dump(screen, screen->esc_params[0]);
    for (;;);

    break;
  }

  screen_flush(screen);
}

static void
screen_out_char(struct Screen *screen, char c)
{
  int i;

  // The first (aka system) console is also connected to the serial port
  if (screen_is_current(screen))
    mach_current->serial_putc(c);

  switch (screen->state) {
  case PARSER_NORMAL:
    if (c == '\x1b')
      screen->state = PARSER_ESC;
    else
      screen_print_char(screen, c);
    break;

	case PARSER_ESC:
		if (c == '[') {
			screen->state = PARSER_CSI;
      screen->esc_cur_param = -1;
      screen->esc_question = 0;
      for (i = 0; i < SCREEN_ESC_MAX; i++)
        screen->esc_params[i] = 0;
		} else {
			screen->state = PARSER_NORMAL;
		}
		break;

	case PARSER_CSI:
    if (c == '?') {
      if (screen->esc_cur_param == -1) {
        screen->esc_question = 1;
      } else {
        screen->state = PARSER_NORMAL;
      }
    } else {
      if (c >= '0' && c <= '9') {
        if (screen->esc_cur_param == -1)
          screen->esc_cur_param = 0;

        // Parse the current parameter
        if (screen->esc_cur_param < SCREEN_ESC_MAX)
          screen->esc_params[screen->esc_cur_param] = screen->esc_params[screen->esc_cur_param] * 10 + (c - '0');
      } else if (c == ';') {
        // Next parameter
        if (screen->esc_cur_param < SCREEN_ESC_MAX)
          screen->esc_cur_param++;
      } else {
        if (screen->esc_cur_param < SCREEN_ESC_MAX)
          screen->esc_cur_param++;

        screen_handle_esc(screen, c);
        screen->state = PARSER_NORMAL;
      }
    }
		break;

	default:
		screen->state = PARSER_NORMAL;
    break;
	}
}

static void
screen_backspace(struct Screen *screen)
{
  k_spinlock_acquire(&screen->lock);
  if (screen->pos > 0)
    screen_set_char(screen, --screen->pos, ' ');
  k_spinlock_release(&screen->lock);
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

static void
screen_echo(struct Screen *screen, char c)
{
  k_spinlock_acquire(&screen->lock);
  screen_out_char(screen, c);
  screen_flush(screen);
  k_spinlock_release(&screen->lock);
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  if (tty_system != NULL)
    screen_echo(tty_system->out, c);
}

// ----------------------------------------------------------------------------

void
tty_switch(int n)
{
  if ((n < 0) || (n >= NTTYS))
    return; // panic?

  if (tty_current != &ttys[n]) {
    tty_current = &ttys[n];
    mach_current->display_update(tty_current->out);
  }
}

// Send a signal to all processes in the given console process group
static void
tty_signal(struct Tty *tty, int signo)
{
  if (tty->pgrp <= 1)
    return;

  k_spinlock_release(&tty->in.lock);

  if (signal_generate(-tty->pgrp, signo, 0) != 0)
    panic("cannot generate signal");

  k_spinlock_acquire(&tty->in.lock);
}

static int
tty_erase_input(struct Tty *tty)
{
  if (tty->in.size == 0)
    return 0;

  if (tty->termios.c_lflag & ECHOE) {
    screen_backspace(tty->out);
    screen_echo(tty->out, '\b');
  }

  tty->in.size--;
  if (tty->in.write_pos == 0) {
    tty->in.write_pos = TTY_INPUT_MAX - 1;
  } else {
    tty->in.write_pos--;
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
          screen_echo(tty->out, c);

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
        k_spinlock_acquire(&tty->out->lock);
        tty->out->stopped = 1;
        k_spinlock_release(&tty->out->lock);

        if (tty->termios.c_iflag & IXOFF)
          screen_echo(tty->out, c);

        continue;
      }

      if ((c == tty->termios.c_cc[VSTART]) || (tty->termios.c_iflag & IXANY)) {
        k_spinlock_acquire(&tty->out->lock);
        tty->out->stopped = 0;
        k_spinlock_release(&tty->out->lock);

        if (c == tty->termios.c_cc[VSTART]) {
          if (tty->termios.c_iflag & IXOFF)
            screen_echo(tty->out, c);
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
        screen_echo(tty->out, c);
        continue;
      }
    }

    if ((c != tty->termios.c_cc[VEOF]) && (tty->termios.c_lflag & ECHO))
      screen_echo(tty->out, c);
    else if ((c == '\n') && (tty->termios.c_lflag & ECHONL))
      screen_echo(tty->out, c);

    if (tty->in.size == (TTY_INPUT_MAX - 1)) {
      // Reserve space for one EOL character at the end of the input buffer
      if (!(tty->termios.c_lflag & ICANON))
        continue;
      if ((c != tty->termios.c_cc[VEOL]) &&
          (c != tty->termios.c_cc[VEOF]) &&
          (c != '\n'))
        continue;
    } else if (tty->in.size == TTY_INPUT_MAX) {
      // Input buffer full - discard all extra characters
      continue;
    }

    tty->in.buf[tty->in.write_pos] = c;
    tty->in.write_pos = (tty->in.write_pos + 1) % TTY_INPUT_MAX;
    tty->in.size++;
  }

  if ((status & (IN_EOF | IN_EOL)) || !(tty->termios.c_lflag & ICANON))
    k_waitqueue_wakeup_all(&tty->in.queue);

  k_spinlock_release(&tty->in.lock);
}

// Use the device minor number to select the virtual console corresponding to
// this inode
static struct Tty *
tty_from_dev(dev_t dev)
{
  // No need to lock since rdev cannot change once we obtain an inode ref
  // TODO: are we sure?
  int minor = dev & 0xFF;
  return minor < NTTYS ? &ttys[minor] : NULL;
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
    tty->in.read_pos = (tty->in.read_pos + 1) % TTY_INPUT_MAX;
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

  k_spinlock_acquire(&tty->out->lock);

  if (!tty->out->stopped) {
    // TODO: unlock periodically to not block the entire system?
    for (i = 0; i != nbytes; i++) {
      int c, r;

      if ((r = vm_space_copy_in(&c, buf + i, 1)) < 0) {
        k_spinlock_release(&tty->out->lock);
        return r;
      }

      screen_out_char(tty->out, c);
    }

    screen_flush(tty->out);
  }

  k_spinlock_release(&tty->out->lock);
  
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
    return tty_current->pgrp;
  case TIOCSPGRP:
    // TODO: validate
    tty_current->pgrp = arg;
    return 0;
  case TIOCGWINSZ:
    ws.ws_col = SCREEN_COLS;
    ws.ws_row = SCREEN_ROWS;
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
