#ifndef __AG_INCLUDE_ARCH_KERNEL_THREAD_H__
#define __AG_INCLUDE_ARCH_KERNEL_THREAD_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <stdint.h>

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct ThreadContext {
  uint32_t s[32];
  uint32_t fpscr;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;
};

void arch_thread_idle(void);
void arch_thread_switch(void **, void *);
void arch_thread_create(struct Thread *);

#endif  // !__AG_INCLUDE_ARCH_KERNEL_THREAD_H__
