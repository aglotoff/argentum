#ifndef __LWIP_OSDEV_ARCH_SYS_ARCH_H__
#define __LWIP_OSDEV_ARCH_SYS_ARCH_H__

#include <kernel/kmutex.h>
#include <kernel/kqueue.h>
#include <kernel/ksemaphore.h>
#include <kernel/task.h>

typedef struct KQueue *sys_mbox_t;
typedef struct KMutex *sys_mutex_t;
typedef struct KSemaphore *sys_sem_t;
typedef struct Task *sys_thread_t;

#endif  /* __LWIP_OSDEV_ARCH_SYS_ARCH_H__ */
