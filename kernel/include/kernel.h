#ifndef __AG_KERNEL_KERNEL_H__
#define __AG_KERNEL_KERNEL_H__

#ifndef __ASSEMBLER__

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern const char *panic_str;

#ifdef __cplusplus
extern "C" {
#endif

void kprintf(const char *, ...);
void vkprintf(const char *, va_list);

#ifdef __cplusplus
};
#endif

#endif  // !__ASSEMBLER__

#endif  // !__AG_KERNEL_KERNEL_H__
