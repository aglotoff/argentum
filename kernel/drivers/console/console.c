#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include <kernel/thread.h>
#include <kernel/spinlock.h>
#include <kernel/mm/memlayout.h>
#include <kernel/mm/vm.h>
#include <kernel/drivers/console.h>
#include <kernel/trap.h>
#include <kernel/wchan.h>
#include <kernel/process.h>

#include "kbd.h"
#include "display.h"
#include "serial.h"

#define CONSOLE_BUF_SIZE  256

static struct {
  char               buf[CONSOLE_BUF_SIZE];
  size_t             size;
  size_t             read_pos;
  size_t             write_pos;
  struct SpinLock    lock;
  struct WaitChannel queue;
} input;

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  spin_init(&input.lock, "input");
  wchan_init(&input.queue);

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
console_interrupt(int (*getc)(void))
{
  int c;

  spin_lock(&input.lock);

  while ((c = getc()) >= 0) {
    switch (c) {
    case '\0':
      break;
    case '\b':
      if (input.size > 0) {
        console_putc(c);

        input.size--;
        // if CONSOLE_BUF_SIZE is guaranteed to be a divisor of UINT_MAX:
        // input.write_pos = (input.write_pos - 1) % CONSOLE_BUF_SIZE;
        if (input.write_pos == 0) {
          input.write_pos = CONSOLE_BUF_SIZE - 1;
        } else {
          input.write_pos--;
        }
      }
      break;
    default:
      if (c != C('D'))
        console_putc(c);

      input.buf[input.write_pos] = c;
      input.write_pos = (input.write_pos + 1) % CONSOLE_BUF_SIZE;

      if (input.size == CONSOLE_BUF_SIZE) {
        input.read_pos = (input.read_pos + 1) % CONSOLE_BUF_SIZE;
      } else {
        input.size++;
      }

      if ((c == '\r') ||
          (c == '\n') ||
          (c == C('D')) ||
          (input.size == CONSOLE_BUF_SIZE))
        wchan_wakeup_all(&input.queue);
      break;
    }
  }

  spin_unlock(&input.lock);
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

  spin_lock(&input.lock);

  while (i < nbytes) {
    while (input.size == 0)
      wchan_sleep(&input.queue, &input.lock);

    c = input.buf[input.read_pos];
    input.read_pos = (input.read_pos + 1) % CONSOLE_BUF_SIZE;
    input.size--;

    // End of input
    if (c == C('D'))
      break;

    // Stop if a newline is encountered
    if ((s[i++] = c) == '\n')
      break;
  }

  spin_unlock(&input.lock);
  
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
