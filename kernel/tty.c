#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/tty.h>
#include <kernel/trap.h>
#include <kernel/process.h>
#include <kernel/core/task.h>
#include <kernel/vmspace.h>
#include <kernel/fs/fs.h>
#include <kernel/console.h>
#include <kernel/time.h>
#include <kernel/dev.h>
#include <kernel/signal.h>
#include <kernel/core/condvar.h>

#include <kernel/drivers/kbd.h>

static struct CharDev tty_device = {
  .open   = tty_open,
  .read   = tty_read,
  .write  = tty_write,
  .ioctl  = tty_ioctl,
  .select = tty_select,
};

#define NTTYS       6   // The total number of virtual ttys

static struct Tty ttys[NTTYS];
struct Tty *tty_current;
struct Tty *tty_system;

#define IN_EOF  (1 << 0)
#define IN_EOL  (1 << 1)

/**
 * Initialize the console devices.
 */
void
tty_init(void)
{  
  int i;

  arch_tty_init_system();

  for (i = 0; i < NTTYS; i++) {
    struct Tty *tty = &ttys[i];

    k_mutex_init(&tty->in.mutex, "tty.in");
    k_condvar_create(&tty->in.cond);

    k_mutex_init(&tty->out.mutex, "tty.out");
    tty->out.stopped = 0;

    arch_tty_init(tty, i);

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

  arch_tty_switch(tty_current);

  dev_register_char(0x01, &tty_device);
  dev_register_char(0x03, &tty_device);
}

static void
tty_echo(struct Tty *tty, char c)
{
  if (k_mutex_lock(&tty->out.mutex) < 0)
    return; // TODO: return?

  arch_tty_out_char(tty, c);
  arch_tty_flush(tty);

  k_mutex_unlock(&tty->out.mutex);
}

void
tty_switch(int n)
{
  if ((n < 0) || (n >= NTTYS))
    return; // panic?

  if (tty_current != &ttys[n]) {
    tty_current = &ttys[n];
    arch_tty_switch(tty_current);
  }
}

// Send a signal to all processes in the given console process group
static void
tty_signal(struct Tty *tty, int signo)
{
  if (tty->pgrp <= 1)
    return;

  k_mutex_unlock(&tty->in.mutex);

  if (signal_generate(-tty->pgrp, signo, 0) != 0)
    k_panic("cannot generate signal");

  // TODO: return
  k_mutex_lock(&tty->in.mutex);
}

static int
tty_erase_input(struct Tty *tty)
{
  if (tty->in.size == 0)
    return 0;

  if (tty->termios.c_lflag & ECHOE) {
    k_mutex_lock(&tty->out.mutex);
    arch_tty_erase(tty);
    k_mutex_unlock(&tty->out.mutex);
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

  k_mutex_lock(&tty->in.mutex);

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
        k_mutex_lock(&tty->out.mutex);
        tty->out.stopped = 1;
        k_mutex_unlock(&tty->out.mutex);

        if (tty->termios.c_iflag & IXOFF)
          tty_echo(tty, c);

        continue;
      }

      if ((c == tty->termios.c_cc[VSTART]) || (tty->termios.c_iflag & IXANY)) {
        k_mutex_lock(&tty->out.mutex);
        tty->out.stopped = 0;
        k_mutex_unlock(&tty->out.mutex);

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
    k_condvar_broadcast(&tty->in.cond);

  k_mutex_unlock(&tty->in.mutex);
}

// Use the device minor number to select the virtual console corresponding to
// this inode
static struct Tty *
tty_from_dev(dev_t dev)
{
  // No need to lock since rdev cannot change once we obtain an inode ref
  // TODO: are we sure?
  int major = (dev >> 8) & 0xFF;
  
  if (major == 0x01) {
    int minor = dev & 0xFF;
    return minor < NTTYS ? &ttys[minor] : NULL;
  }

  if (major == 0x03) {
    struct Process *process = process_current();

    if (((process->ctty >> 8) & 0xFF) == 0x01) {
      int minor = dev & 0xFF;
      return minor < NTTYS ? &ttys[minor] : NULL;
    }
  }

  return NULL;
}

int
tty_open(struct Thread *thread, dev_t dev, int oflag, mode_t)
{
  struct Tty *tty = tty_from_dev(dev);
  struct Process *my_process = thread->process;

  if (tty == NULL)
    return -ENODEV;

  if (!(oflag & O_NOCTTY) && (my_process->ctty == 0))
    my_process->ctty = dev;

  return 0;
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
tty_read(struct Thread *thread, dev_t dev, uintptr_t buf, size_t nbytes)
{
  struct Tty *tty = tty_from_dev(dev);
  size_t i = 0;

  if (tty == NULL)
    return -ENODEV;

  k_mutex_lock(&tty->in.mutex);

  while (i < nbytes) {
    char c;
    int r;

    // Wait for input
    while (tty->in.size == 0) {
      if ((r = k_condvar_wait(&tty->in.cond, &tty->in.mutex)) < 0) {
        k_mutex_unlock(&tty->in.mutex);
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

      if ((r = vm_space_copy_out(thread, &c, buf++, 1)) < 0) {
        k_mutex_unlock(&tty->in.mutex);
        return r;
      }
      i++;

      // In canonical mode, we process at most a single line of input
      if ((c == tty->termios.c_cc[VEOL]) || (c == '\n'))
        break;
    } else {
      if ((r = vm_space_copy_out(thread, &c, buf++, 1)) < 0) {
        k_mutex_unlock(&tty->in.mutex);
        return r;
      }
      i++;

      if (i >= tty->termios.c_cc[VMIN])
        break;
    }
  }

  k_mutex_unlock(&tty->in.mutex);

  return i;
}

ssize_t
tty_write(struct Thread *thread, dev_t dev, uintptr_t buf, size_t nbytes)
{
  struct Tty *tty = tty_from_dev(dev);
  size_t i;

  if (tty == NULL)
    return -ENODEV;

  k_mutex_lock(&tty->out.mutex);

  if (!tty->out.stopped) {
    // TODO: unlock periodically to not block the entire system?
    for (i = 0; i != nbytes; i++) {
      int c, r;

      if ((r = vm_space_copy_in(thread, &c, buf + i, 1)) < 0) {
        k_mutex_unlock(&tty->out.mutex);
        return r;
      }

      arch_tty_out_char(tty, c);
    }

    arch_tty_flush(tty);
  }

  k_mutex_unlock(&tty->out.mutex);
  
  return i;
}

int
tty_ioctl(struct Thread *thread, dev_t dev, int request, int arg)
{
  struct Tty *tty = tty_from_dev(dev);
  struct winsize ws;

  if (tty == NULL)
    return -ENODEV;

  switch (request) {
  case TIOCGETA:
    return vm_copy_out(thread->process->vm->pgtab,
                       &tty->termios,
                       arg,
                       sizeof(struct termios));

  case TIOCSETAW:
    // TODO: drain
  case TIOCSETA:
    return vm_copy_in(thread->process->vm->pgtab,
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
    ws.ws_col = tty_current->out.screen->cols;
    ws.ws_row = tty_current->out.screen->rows;
    ws.ws_xpixel = tty_current->out.screen->cols * 8;  // FIXME
    ws.ws_ypixel = tty_current->out.screen->rows * 16; // FIXME
    return vm_copy_out(thread->process->vm->pgtab, &ws, arg, sizeof ws);
  case TIOCSWINSZ:
    // cprintf("set winsize\n");
    if (vm_copy_in(thread->process->vm->pgtab, &ws, arg, sizeof ws) < 0)
      return -EFAULT;
    // cprintf("ws_col = %d\n", ws.ws_col);
    // cprintf("ws_row = %d\n", ws.ws_row);
    // cprintf("ws_xpixel = %d\n", ws.ws_xpixel);
    // cprintf("ws_ypixel = %d\n", ws.ws_ypixel);
    return 0;
  default:
    k_panic("TODO: %p - %d %c %d\n", request, request & 0xFF, (request >> 8) & 0xF, (request >> 16) & 0x1FFF);
    return -EINVAL;
  }
}

static int
tty_try_select(struct Thread *, struct Tty *tty)
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
tty_select(struct Thread *thread, dev_t dev, struct timeval *timeout)
{
  struct Tty *tty = tty_from_dev(dev);
  int r;

  if (tty == NULL)
    return -ENODEV;

  k_mutex_lock(&tty->in.mutex);

  while ((r = tty_try_select(thread, tty)) == 0) {
    unsigned long t = 0;

    if (timeout != NULL) {
      t = timeval2ticks(timeout);
      k_mutex_unlock(&tty->in.mutex);
      return 0;
    }

    // FIXME: use timer to wakeup thread
    if ((r = k_condvar_timed_wait(&tty->in.cond, &tty->in.mutex, t)) < 0) {
      k_mutex_unlock(&tty->in.mutex);
      return r;
    }
  }

  k_mutex_unlock(&tty->in.mutex);

  return r;
}
