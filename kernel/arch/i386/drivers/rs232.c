#include <stddef.h>

#include <kernel/drivers/uart.h>

#include <arch/trap.h>
#include <arch/i386/io.h>

#define COM1    0x3f8

static int rs232_read(void *);
static int rs232_write(void *, int);

static struct Uart uart;

struct UartOps rs232_ops = {
  .read  = rs232_read,
  .write = rs232_write,
};

int
rs232_init(void)
{
  // Turn off the FIFO
  outb(COM1+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(COM1+3, 0x80);    // Unlock divisor
  outb(COM1+0, 115200/9600);
  outb(COM1+1, 0);
  outb(COM1+3, 0x03);    // Lock divisor, 8 data bits.
  outb(COM1+4, 0);
  outb(COM1+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if (inb(COM1+5) == 0xFF)
    return 0;
  
  uart_init(&uart, &rs232_ops, NULL, IRQ_COM1);

  return 1;
}

static int
rs232_read(void *)
{
  if (!(inb(COM1+5) & 0x01))
    return -1;

  return inb(COM1+0);
}

static int
rs232_write(void *arg, int c)
{
  (void) arg;

  while (!(inb(COM1+5) & 0x20))
    ;

  outb(COM1+0, c);

  return 0;
}

int
rs232_putc(int c)
{
  return uart_putc(&uart, c);
}

int
rs232_getc(void)
{
  return uart_getc(&uart);
}
