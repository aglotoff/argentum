#ifndef __AG_KERNEL_ARCH_CONSOLE_H__
#define __AG_KERNEL_ARCH_CONSOLE_H__

void arch_console_init(void);
void arch_console_putc(char);
int  arch_console_getc(void);

#endif  // !__AG_KERNEL_CONSOLE_H__
