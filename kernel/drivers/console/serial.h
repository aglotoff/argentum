#ifndef __KERNEL_DRIVERS_CONSOLE_SERIAL_H__
#define __KERNEL_DRIVERS_CONSOLE_SERIAL_H__

void serial_init(void);
int  serial_getc(void);
void serial_putc(char);

#endif  // !__KERNEL_DRIVERS_CONSOLE_SERIAL_H__
