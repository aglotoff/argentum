#ifndef __ARCH_ARM_CONTEXT_H__
#define __ARCH_ARM_CONTEXT_H__

#include <stdint.h>

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct Context {
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

#endif  // !__ARCH_ARM_CONTEXT_H__
