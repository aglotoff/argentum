#ifndef __LWIP_OSDEV_ARCH_SYS_ARCH_H__
#define __LWIP_OSDEV_ARCH_SYS_ARCH_H__

#include <kernel/core/mutex.h>
#include <kernel/core/mailbox.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/task.h>

typedef struct KMailBox *sys_mbox_t;
typedef struct KMutex *sys_mutex_t;
typedef struct KSemaphore *sys_sem_t;
typedef struct KTask *sys_thread_t;

#endif  /* __LWIP_OSDEV_ARCH_SYS_ARCH_H__ */
