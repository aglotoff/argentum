#ifndef __LWIP_OSDEV_ARCH_SYS_ARCH_H__
#define __LWIP_OSDEV_ARCH_SYS_ARCH_H__

#include <kernel/mutex.h>
#include <kernel/mailbox.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>

typedef struct KMailBox *sys_mbox_t;
typedef struct KMutex *sys_mutex_t;
typedef struct KSemaphore *sys_sem_t;
typedef struct KThread *sys_thread_t;

#endif  /* __LWIP_OSDEV_ARCH_SYS_ARCH_H__ */
