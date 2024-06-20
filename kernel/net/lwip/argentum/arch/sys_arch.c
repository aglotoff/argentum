#include <kernel/cpu.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/mutex.h>
#include <kernel/mailbox.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>
#include <kernel/tick.h>
#include <kernel/types.h>
#include <lwip/sys.h>

#include "cc.h"
#include "sys_arch.h"

// Not good in multithreaded environment!

static struct KObjectPool *queue_cache;

/* Mutex functions: */
err_t
sys_mutex_new(sys_mutex_t *mutex)
{
  struct KMutex *kmutex;

  if ((kmutex = k_mutex_create("lwip")) == NULL)
    return ERR_MEM;

  *mutex = kmutex;
  return ERR_OK;
}

void
sys_mutex_lock(sys_mutex_t *mutex)
{
  k_mutex_lock(*mutex);
}

void
sys_mutex_unlock(sys_mutex_t *mutex)
{
  k_mutex_unlock(*mutex);
}

void
sys_mutex_free(sys_mutex_t *mutex)
{
  k_mutex_destroy(*mutex);
}

int
sys_mutex_valid(sys_mutex_t *mutex)
{
  return *mutex != NULL;
}

void
sys_mutex_set_invalid(sys_mutex_t *mutex)
{
  *mutex = NULL;
}

/* Semaphore functions: */

err_t
sys_sem_new(sys_sem_t *sem, u8_t count)
{
  struct KSemaphore *ksemaphore;

  if ((ksemaphore = k_semaphore_create(count)) == NULL)
    return ERR_MEM;

  *sem = ksemaphore;
  return ERR_OK;
}

void
sys_sem_signal(sys_sem_t *sem)
{
  k_semaphore_put(*sem);
}

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
  unsigned long start, end;

  start = tick_get();
  if (k_semaphore_timed_get(*sem, timeout / MS_PER_TICK) < 0)
    return SYS_ARCH_TIMEOUT;
  end = tick_get();
  
  return MIN(timeout, (end - start) * MS_PER_TICK);
}

void
sys_sem_free(sys_sem_t *sem)
{
  k_semaphore_destroy(*sem);
}

int
sys_sem_valid(sys_sem_t *sem)
{
  return *sem != NULL;
}

void
sys_sem_set_invalid(sys_sem_t *sem)
{
  *sem = NULL;
}

/* Mailbox functions. */

err_t
sys_mbox_new(sys_mbox_t *mbox, int size)
{
  struct KMailBox *queue;
  struct Page *page;

  (void) size;

  if ((queue = (struct KMailBox *) k_object_pool_get(queue_cache)) == NULL)
    panic("k_object_pool_get");
  if ((page = page_alloc_one(0)) == NULL)
    panic("page_alloc");
  if (k_mailbox_init(queue, sizeof(void *), page2kva(page), PAGE_SIZE) < 0)
    panic("k_mailbox_init");

  *mbox = queue;

  return 0;
}

void
sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
  k_mailbox_send(*mbox, &msg, 0, 1);
}

err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
  return k_mailbox_send(*mbox, &msg, 0, 0);
}

err_t
sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
  return k_mailbox_send(*mbox, &msg, 0, 0);
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
  unsigned long start, end;

  start = tick_get();
  if (k_mailbox_receive(*mbox, msg, timeout / 10, 1) < 0)
    return SYS_ARCH_TIMEOUT;
  end = tick_get();
  
  return MIN(timeout, (end - start) * 10);
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
  if (k_mailbox_receive(*mbox, msg, 0, 0) < 0)
    return SYS_MBOX_EMPTY;
  return 0;
}

void
sys_mbox_free(sys_mbox_t *mbox)
{
  k_object_pool_put(queue_cache, *mbox);
}

int
sys_mbox_valid(sys_mbox_t *mbox)
{
  return *mbox != NULL;
}

void
sys_mbox_set_invalid(sys_mbox_t *mbox)
{
  *mbox = NULL;
}

sys_thread_t
sys_thread_new(const char *name, void (*thread)(void *), void *arg,
               int stacksize, int prio)
{
  struct KThread *task;

  // TODO: priority!
  (void) name;
  (void) prio;
  (void) stacksize;

  task = k_thread_create(NULL, thread, arg, 0);
  k_thread_resume(task);

  return task;
}

int *
__errno(void)
{
  return &k_thread_current()->err;
}

void
sys_init(void)
{
  queue_cache = k_object_pool_create("queue", sizeof(struct KMailBox), 0, NULL, NULL);
}

int errno;

u32_t
sys_jiffies(void)
{
  return tick_get();
}

u32_t
sys_now(void)
{
  return tick_get() * 10;
}

static struct KSpinLock lwip_lock = K_SPINLOCK_INITIALIZER("lwip");

sys_prot_t
sys_arch_protect(void)
{
  k_spinlock_acquire(&lwip_lock);
  return 0;
}

void
sys_arch_unprotect(sys_prot_t pval)
{
  (void) pval;
  k_spinlock_release(&lwip_lock);
}
