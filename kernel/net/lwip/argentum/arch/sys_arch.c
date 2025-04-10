#include <kernel/core/cpu.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/core/mutex.h>
#include <kernel/core/mailbox.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/task.h>
#include <kernel/time.h>
#include <kernel/types.h>
#include <lwip/sys.h>

#include "cc.h"
#include "sys_arch.h"

/* Mutex functions: */
err_t
sys_mutex_new(sys_mutex_t *mutex)
{
  struct KMutex *kmutex;

  if ((kmutex = k_malloc(sizeof(struct KMutex))) == NULL)
    return ERR_MEM;
  
  k_mutex_init(kmutex, "lwip");

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
  k_mutex_fini(*mutex);
  k_free(*mutex);
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

  if ((ksemaphore = k_malloc(sizeof(struct KSemaphore))) == NULL)
    return ERR_MEM;

  k_semaphore_create(ksemaphore, count);

  *sem = ksemaphore;
  return ERR_OK;
}

void
sys_sem_signal(sys_sem_t *sem)
{
  k_semaphore_put(*sem);
}

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms)
{
  unsigned long start_ticks, end_ticks;

  start_ticks = k_tick_get();
  if (k_semaphore_timed_get(*sem, ms2ticks(timeout_ms), K_SLEEP_UNINTERUPTIBLE) < 0)
    return SYS_ARCH_TIMEOUT; 
  end_ticks = k_tick_get();
  
  return MIN(timeout_ms, ticks2ms(end_ticks - start_ticks));
}

void
sys_sem_free(sys_sem_t *sem)
{
  k_semaphore_destroy(*sem);
  k_free(*sem);
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
  struct KMailBox *kmailbox;
  void *buf;
  
  if ((kmailbox = k_malloc(sizeof(struct KMailBox))) == NULL)
    return ERR_MEM;

  (void) size;
  
  if ((buf = k_malloc(PAGE_SIZE)) == NULL) {
    k_free(kmailbox);
    return ERR_MEM;
  }

  k_mailbox_create(kmailbox, sizeof(void *), buf, PAGE_SIZE);

  *mbox = kmailbox;
  return ERR_OK;
}

void
sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
  k_mailbox_timed_send(*mbox, &msg, 0);
}

err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
  return k_mailbox_try_send(*mbox, &msg);
}

err_t
sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
  return k_mailbox_try_send(*mbox, &msg);
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout_ms)
{
  unsigned long start_ticks, end_ticks;

  start_ticks = k_tick_get();
  if (k_mailbox_timed_receive(*mbox, msg, ms2ticks(timeout_ms)) < 0)
    return SYS_ARCH_TIMEOUT;
  end_ticks = k_tick_get();

  return MIN(timeout_ms, ticks2ms(end_ticks - start_ticks));
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
  if (k_mailbox_try_receive(*mbox, msg) < 0)
    return SYS_MBOX_EMPTY;
  return 0;
}

void
sys_mbox_free(sys_mbox_t *mbox)
{
  k_mailbox_destroy(*mbox);
  k_free((*mbox)->buf_start);
  k_free(*mbox);
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
  struct Page *stack_page;
  struct KTask *task;
  uint8_t *stack;

  if ((stack_page = page_alloc_one(0, PAGE_TAG_KSTACK)) == NULL)
    k_panic("cannot create IRQ task");

  stack = (uint8_t *) page2kva(stack_page);
  stack_page->ref_count++;

  // TODO: priority!
  (void) name;
  (void) prio;
  (void) stacksize;

  if ((task = k_malloc(sizeof(struct KTask))) == NULL)
    k_panic("cannot create IRQ task");

  k_task_create(task, NULL, thread, arg, stack, PAGE_SIZE, 0);
  k_task_resume(task);

  return task;
}

int *
__errno(void)
{
  return &k_task_current()->err;
}

void
sys_init(void)
{
}

int errno;

u32_t
sys_jiffies(void)
{
  return k_tick_get();
}

u32_t
sys_now(void)
{
  return k_tick_get() * 10;
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
