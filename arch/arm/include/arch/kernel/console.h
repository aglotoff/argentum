#ifndef __AG_INCLUDE_ARCH_KERNEL_CONSOLE_H__
#define __AG_INCLUDE_ARCH_KERNEL_CONSOLE_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

void arch_console_init(void);
void arch_console_putc(char);
int  arch_console_getc(void);

#endif  // !__AG_INCLUDE_ARCH_KERNEL_CONSOLE_H__
