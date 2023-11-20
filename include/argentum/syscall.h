#ifndef __AG_INCLUDE_ARGENTUM_SYSCALL_H__
#define __AG_INCLUDE_ARGENTUM_SYSCALL_H__

#include <errno.h>
#include <stddef.h>
#include <arch/syscall.h>

// System call numbers
#define __SYS_EXIT        0
#define __SYS_CWRITE      1

void cwrite(const void *, size_t);

#endif  // !__AG_INCLUDE_ARGENTUM_SYSCALL_H__
