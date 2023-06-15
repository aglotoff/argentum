#ifndef __LWIP_ARGENTUM_ARCH_CC_H__
#define __LWIP_ARGENTUM_ARCH_CC_H__

#include <assert.h>
#include <kernel/cprintf.h>
#include <stdlib.h>

typedef int sys_prot_t;

#define LWIP_PLATFORM_ASSERT(x) panic(x)

#define LWIP_RAND() ((u32_t)rand())

#endif  /* __LWIP_ARGENTUM_ARCH_CC_H__ */
