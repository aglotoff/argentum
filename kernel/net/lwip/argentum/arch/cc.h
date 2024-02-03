#ifndef __LWIP_OSDEV_ARCH_CC_H__
#define __LWIP_OSDEV_ARCH_CC_H__

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/time.h>

#include <kernel/assert.h>
#include <kernel/cprintf.h>

#define LWIP_NO_CTYPE_H         1

typedef int sys_prot_t;

#define LWIP_PLATFORM_ASSERT(x) panic(x)

#define LWIP_RAND() ((u32_t)rand())

#endif  /* __LWIP_OSDEV_ARCH_CC_H__ */
