#include <stdint.h>
#include <stdio.h>

#include <kernel/armv7.h>
#include <kernel/drivers/kbd.h>
#include <kernel/drivers/lcd.h>
#include <kernel/drivers/uart.h>
#include <kernel/monitor.h>
#include <kernel/process.h>
#include <kernel/sync.h>

#include <kernel/drivers/console.h>

static struct {
  struct SpinLock lock;
  int             locking;
} console;

#define CONSOLE_BUF_SIZE  256

static struct {
  char            buf[CONSOLE_BUF_SIZE];
  uint32_t        rpos;
  uint32_t        wpos;
  struct ListLink queue;
} input;

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  spin_init(&console.lock, "console");
  console.locking = 1;

  list_init(&input.queue);

  uart_init();
  kbd_init();
  lcd_init();
}


/*
 * ----------------------------------------------------------------------------
 * Console input
 * ----------------------------------------------------------------------------
 */

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
console_intr(int (*getc)(void))
{
  int c;

  if (console.locking)
    spin_lock(&console.lock);

  while ((c = getc()) >= 0) {
    switch (c) {
    case 0:
      continue;
    case '\b':
      if (input.rpos != input.wpos) {
        console_putc(c);
        input.wpos--;
      }
      break;
    default:
      if (input.wpos == input.rpos + CONSOLE_BUF_SIZE)
        input.rpos++;

      console_putc(c);
      input.buf[input.wpos++ % CONSOLE_BUF_SIZE] = c;

      if ((c == '\r') ||
          (c == '\n') ||
          (input.wpos == input.rpos + CONSOLE_BUF_SIZE))
        process_wakeup(&input.queue);
      break;
    }
  }

  if (console.locking)
    spin_unlock(&console.lock);
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
  do {
    // Poll for any pending characters from UART and the keyboard.
    uart_intr();
    kbd_intr();
  } while (input.rpos == input.wpos);

  return input.buf[input.rpos++ % CONSOLE_BUF_SIZE];
}

ssize_t
console_read(void *buf, size_t nbytes)
{
  char c, *s;
  size_t i;
  
  s = (char *) buf;
  i = 0;
  spin_lock(&console.lock);

  while (i < nbytes) {
    while (input.rpos == input.wpos) {
      process_sleep(&input.queue, &console.lock);
    }

    c = input.buf[input.rpos++ % CONSOLE_BUF_SIZE];

    if ((s[i++] = c) == '\n')
      break;
  }

  spin_unlock(&console.lock);
  
  return i;
}


/*
 * ----------------------------------------------------------------------------
 * Console output
 * ----------------------------------------------------------------------------
 */

static int cur_pos;                 // Current cursor position
static int cur_fg = COLOR_WHITE;    // Current foreground color
static int cur_bg = COLOR_BLACK;    // Current background color

#define ESC_MAX_PARAM   16            // The maximum number of esc parameters

static int esc_state = 0;             // State of the esc sequence parser
static int esc_params[ESC_MAX_PARAM]; // The esc sequence parameters
static int esc_cur_param;             // Index of the current esc parameter

static void console_handle_esc(char);
static void console_parse_esc(char);

static void
console_scroll_screen(void)
{
  cur_pos -= BUF_WIDTH;

  lcd_copy(0, BUF_WIDTH, BUF_SIZE - BUF_WIDTH);
  lcd_fill(BUF_SIZE - BUF_WIDTH, BUF_WIDTH, cur_fg, cur_bg);
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  uart_putc(c);

  if (esc_state) {
    console_parse_esc(c);
    return;
  }

  switch (c) {
  case '\x1b':
    // Beginning of an escape sequence
    esc_state = c;
    break;

  case '\n':
    cur_pos += BUF_WIDTH;
    // fall through
  case '\r':
    cur_pos -= cur_pos % BUF_WIDTH;
    break;

  case '\b':
    if (cur_pos > 0)
      lcd_putc(--cur_pos, ' ', cur_fg, cur_bg);
    break;

  case '\t':
    do {
      lcd_putc(cur_pos++, ' ', cur_fg, cur_bg);
    } while ((cur_pos % 4) != 0);
    break;

  default:
    lcd_putc(cur_pos++, c, cur_fg, cur_bg);
    break;
  }

  if (cur_pos >= BUF_SIZE)
    console_scroll_screen();

  lcd_move_cursor(cur_pos);
}

ssize_t
console_write(const void *buf, size_t nbytes)
{
  const char *s = (const char *) buf;
  size_t i;

  for (i = 0; i != nbytes; i++)
    console_putc(s[i]);
  
  return i;
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

// Handle the escape sequence terminated by the final character c.
static void
console_handle_esc(char c)
{
  int i, tmp;

  switch (c) {
  // Set Graphic Rendition
  case 'm':       
    for (i = 0; (i <= esc_cur_param) && (i < ESC_MAX_PARAM); i++) {
      switch (esc_params[i]) {
      case 0:
        // All attributes off
        cur_bg = COLOR_BLACK;
        cur_fg = COLOR_WHITE;
        break;
      case 1:
        // Bold on
        cur_fg |= COLOR_BRIGHT;
        break;
      case 7:
        // Reverse video
        tmp = cur_bg;
        cur_bg = cur_fg;
        cur_fg = tmp;
        break;
      case 39:
        // Default foreground color (white)
        cur_fg = (cur_fg & ~COLOR_MASK) | COLOR_WHITE;
        break;
      case 49:
        // Default background color (black)
        cur_bg = (cur_bg & ~COLOR_MASK) | COLOR_BLACK;
        break;
      default:
        if ((esc_params[i] >= 30) && (esc_params[i] <= 37)) {
          // Set foreground color
          cur_fg = (cur_fg & ~COLOR_MASK) | (esc_params[i] - 30);
        } else if ((esc_params[i] >= 40) && (esc_params[i] <= 47)) {
          // Set background color
          cur_bg = (cur_bg & ~COLOR_MASK) | (esc_params[i] - 40);
        }
      }
    }
    break;

  // TODO: handle other control sequences here

  default:
    break;
  }

  esc_state = 0;
}

// Process the next character in the current escape sequence.
static void
console_parse_esc(char c)
{
  int i;

  switch (esc_state) {
	case '\x1b':
		if (c == '[') {
			esc_state = c;
      esc_cur_param = 0;
      for (i = 0; i < ESC_MAX_PARAM; i++)
        esc_params[i] = 0;
		} else {
			esc_state = 0;
		}
		break;

	case '[':
		if (c >= '0' && c <= '9') {
			// Parse the current parameter
			if (esc_cur_param < ESC_MAX_PARAM)
        esc_params[esc_cur_param] = esc_params[esc_cur_param] * 10 + (c - '0');
		} else if (c == ';') {
			// Next parameter
			if (esc_cur_param < ESC_MAX_PARAM)
				esc_cur_param++;
		} else {
			console_handle_esc(c);
		}
		break;

	default:
		esc_state = 0;
    break;
	}
}


/*
 * ----------------------------------------------------------------------------
 * Formatted output
 * ----------------------------------------------------------------------------
 */

static int
cputc(void *arg, int c)
{
  (void) arg;
  console_putc(c);
  return 1;
}

/**
 * Printf-like formatted output to the console.
 * 
 * @param format The format string.
 * @param ap     A variable argument list.
 */
void
vcprintf(const char *format, va_list ap)
{
  if (console.locking)
    spin_lock(&console.lock);

  __printf(cputc, NULL, format, ap);
  
  if (console.locking)
    spin_unlock(&console.lock);
}

/**
 * Printf-like formatted output to the console.
 * 
 * @param format The format string.
 */
void
cprintf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vcprintf(format, ap);
  va_end(ap);
}

const char *panicstr;

void
__panic(const char *file, int line, const char *format, ...)
{
  va_list ap;

  if (!panicstr) {
    panicstr = format;
    console.locking = 0;

    cprintf("kernel panic at %s:%d: ", file, line);

    va_start(ap, format);
    vcprintf(format, ap);
    va_end(ap);

    cprintf("\n");
  }

  // Never returns.
  for (;;)
    monitor(NULL);
}

void
__warn(const char *file, int line, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  
  cprintf("kernel warning at %s:%d: ", file, line);
  vcprintf(format, ap);
  cprintf("\n");

  va_end(ap);
}
