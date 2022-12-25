#include <stdint.h>

#include <argentum/kthread.h>
#include <argentum/spin.h>
#include <argentum/mm/memlayout.h>
#include <argentum/mm/vm.h>
#include <argentum/drivers/console.h>
#include <argentum/trap.h>

#include "kbd.h"
#include "display.h"
#include "serial.h"

#define CONSOLE_BUF_SIZE  256

static struct {
  char            buf[CONSOLE_BUF_SIZE];
  uint32_t        rpos;
  uint32_t        wpos;
  struct SpinLock lock;
  struct ListLink queue;
} input;

/**
 * Initialize the console devices.
 */
void
console_init(void)
{  
  spin_init(&input.lock, "input");
  list_init(&input.queue);

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
      if (input.rpos != input.wpos) {
        console_putc(c);
        input.wpos--;
      }
      break;
    default:
      if (input.wpos == input.rpos + CONSOLE_BUF_SIZE)
        input.rpos++;

      if (c != C('D'))
        console_putc(c);

      input.buf[input.wpos++ % CONSOLE_BUF_SIZE] = c;

      if ((c == '\r') ||
          (c == '\n') ||
          (c == C('D')) ||
          (input.wpos == input.rpos + CONSOLE_BUF_SIZE))
        kthread_wakeup(&input.queue);
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
console_read(void *buf, size_t nbytes)
{
  char c, *s;
  size_t i;
  
  s = (char *) buf;
  i = 0;
  spin_lock(&input.lock);

  while (i < nbytes) {
    while (input.rpos == input.wpos)
      kthread_sleep(&input.queue, &input.lock);

    c = input.buf[input.rpos++ % CONSOLE_BUF_SIZE];

    if (c == C('D')) {
      if (i > 0)
        input.rpos--;
      break;
    }

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
