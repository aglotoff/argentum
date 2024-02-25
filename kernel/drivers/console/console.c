#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/drivers/console.h>
#include <kernel/trap.h>
#include <kernel/process.h>
#include <kernel/thread.h>

#include "kbd.h"
#include "display.h"
#include "serial.h"

struct Console console_current;

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  spin_init(&console_current.in.lock, "input");
  wchan_init(&console_current.in.queue);

  serial_init();
  kbd_init();
  display_init();
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
  int c;

  spin_lock(&console_current.in.lock);

  while ((c = *buf++) != 0) {
    switch (c) {
    case '\0':
      break;
    case '\b':
      if (console_current.in.size > 0) {
        console_putc(c);

        console_current.in.size--;
        if (console_current.in.write_pos == 0) {
          console_current.in.write_pos = CONSOLE_INPUT_MAX - 1;
        } else {
          console_current.in.write_pos--;
        }
      }
      break;
    default:
      if (c != C('D'))
        console_putc(c);

      console_current.in.buf[console_current.in.write_pos] = c;
      console_current.in.write_pos = (console_current.in.write_pos + 1) % CONSOLE_INPUT_MAX;

      if (console_current.in.size == CONSOLE_INPUT_MAX) {
        console_current.in.read_pos = (console_current.in.read_pos + 1) % CONSOLE_INPUT_MAX;
      } else {
        console_current.in.size++;
      }

      if ((c == '\r') ||
          (c == '\n') ||
          (c == C('D')) ||
          (console_current.in.size == CONSOLE_INPUT_MAX))
        wchan_wakeup_all(&console_current.in.queue);
      break;
    }
  }

  spin_unlock(&console_current.in.lock);
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
console_read(uintptr_t buf, size_t nbytes)
{
  char c, *s;
  size_t i;
  
  s = (char *) buf;
  i = 0;

  spin_lock(&console_current.in.lock);

  while (i < nbytes) {
    while (console_current.in.size == 0)
      wchan_sleep(&console_current.in.queue, &console_current.in.lock);

    c = console_current.in.buf[console_current.in.read_pos];
    console_current.in.read_pos = (console_current.in.read_pos + 1) % CONSOLE_INPUT_MAX;
    console_current.in.size--;

    // End of input
    if (c == C('D'))
      break;

    // Stop if a newline is encountered
    if ((s[i++] = c) == '\n')
      break;
  }

  spin_unlock(&console_current.in.lock);
  
  return i;
}

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */
void
console_putc(char c)
{
  serial_putc(c);
  display_putc(c);
  display_flush();
}

ssize_t
console_write(const void *buf, size_t nbytes)
{
  const char *s = (const char *) buf;
  size_t i;

  for (i = 0; i != nbytes; i++) {
    serial_putc(s[i]);
    display_putc(s[i]);
  }
  
  display_flush();
  
  return i;
}

static int pgrp;

int
console_ioctl(int request, int arg)
{
  switch (request) {
  case TIOCGPGRP:
    return pgrp;
  case TIOCSPGRP:
    // TODO: validate
    pgrp = arg;
    return 0;

  default:
    for (;;);
    return -EINVAL;
  }
}
