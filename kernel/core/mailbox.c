#include <errno.h>
#include <string.h>

#include <kernel/cpu.h>
#include <kernel/mailbox.h>
#include <kernel/thread.h>
#include <kernel/types.h>
#include <kernel/cprintf.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>

#include "core_private.h"

static void k_mailbox_ctor(void *, size_t);
static void k_mailbox_dtor(void *, size_t);
static void k_mailbox_init_common(struct KMailBox *, size_t, void *, size_t);

static struct KObjectPool *k_mailbox_pool;

void
k_mailbox_system_init(void)
{
  if ((k_mailbox_pool = k_object_pool_create("mailbox",
                                             sizeof(struct KMailBox),
                                             0,
                                             k_mailbox_ctor,
                                             k_mailbox_dtor)) == NULL)
    panic("cannot create mailbox pool");
}

struct KMailBox *
k_mailbox_create(size_t msg_size, size_t size)
{
  struct KMailBox *mbox;
  struct Page *page;
  
  if ((mbox = (struct KMailBox *) k_object_pool_get(k_mailbox_pool)) == NULL)
    return NULL;
  
  if ((size * msg_size) > PAGE_SIZE)
    panic("size too large");
  
  if ((page = page_alloc_one(0)) == NULL) {
    k_object_pool_put(k_mailbox_pool, mbox);
    return NULL;
  }

  page->ref_count++;

  k_mailbox_init_common(mbox, msg_size, page2kva(page), PAGE_SIZE);

  return mbox;
}

void
k_mailbox_destroy(struct KMailBox *mbox)
{
  struct Page *page;

  page = kva2page(mbox->buf_start);
  if (--page->ref_count == 0)
    page_free_one(page);

  k_mailbox_fini(mbox);
}

int
k_mailbox_init(struct KMailBox *mbox, size_t msg_size, void *start, size_t size)
{
  k_mailbox_ctor(mbox, sizeof(struct KMailBox));
  k_mailbox_init_common(mbox, msg_size, start, size);
  return 0;
}

int
k_mailbox_fini(struct KMailBox *mbox)
{
  k_spinlock_acquire(&mbox->lock);
  _k_sched_wakeup_all(&mbox->receive_list, -EINVAL);
  _k_sched_wakeup_all(&mbox->send_list, -EINVAL);
  k_spinlock_release(&mbox->lock);

  return 0;
}

int
k_mailbox_receive(struct KMailBox *mbox, void *msg, unsigned long timeout,
                  int blocking)
{
  struct KThread *my_task = k_thread_current();
  int r;

  if ((my_task == NULL) && blocking)
    // TODO: choose another value to indicate an error?
    return -EAGAIN;

  k_spinlock_acquire(&mbox->lock);

  while (mbox->size == 0) {
    struct KCpu *my_cpu = _k_cpu();

    if (!blocking || (my_cpu->lock_count > 0)) {
      // Can't block
      k_spinlock_release(&mbox->lock);
      return -EAGAIN;
    }

    if ((r = _k_sched_sleep(&mbox->receive_list, THREAD_STATE_SLEEP, timeout, &mbox->lock)) != 0) {
      k_spinlock_release(&mbox->lock);;
      return r;
    }
  }

  memmove(msg, mbox->read_ptr, mbox->msg_size);

  mbox->read_ptr += mbox->msg_size;
  if (mbox->read_ptr >= mbox->buf_end)
    mbox->read_ptr = mbox->buf_start;

  if (mbox->size-- == mbox->max_size)
    _k_sched_wakeup_one(&mbox->send_list, 0);

  k_spinlock_release(&mbox->lock);

  return r;
}

int
k_mailbox_send(struct KMailBox *mbox, const void *msg, unsigned long timeout,
            int blocking)
{
  struct KThread *my_task = k_thread_current();
  int ret;

  if ((my_task == NULL) && blocking)
    // TODO: choose another value to indicate an error?
    return -EAGAIN;

  k_spinlock_acquire(&mbox->lock);

  while (mbox->size == mbox->max_size) {
    struct KCpu *my_cpu = _k_cpu();

    if (!blocking || (my_cpu->lock_count > 0)) {
      // Can't block
      k_spinlock_release(&mbox->lock);
      return -EAGAIN;
    }

    _k_sched_sleep(&mbox->send_list, THREAD_STATE_SLEEP, timeout, &mbox->lock);

    if ((ret = my_task->sleep_result) != 0) {
      k_spinlock_release(&mbox->lock);
      return ret;
    }
  }

  memmove(mbox->write_ptr, msg, mbox->msg_size);

  mbox->write_ptr += mbox->msg_size;
  if (mbox->write_ptr >= mbox->buf_end)
    mbox->write_ptr = mbox->buf_start;

  if (mbox->size++ == 0)
    _k_sched_wakeup_one(&mbox->receive_list, 0);

  k_spinlock_release(&mbox->lock);

  return ret;
}

static void
k_mailbox_ctor(void *p, size_t n)
{
  struct KMailBox *mbox = (struct KMailBox *) p;
  (void) n;

  k_spinlock_init(&mbox->lock, "k_mailbox");
  k_list_init(&mbox->receive_list);
  k_list_init(&mbox->send_list);
}

static void
k_mailbox_dtor(void *p, size_t n)
{
  struct KMailBox *mbox = (struct KMailBox *) p;
  (void) n;

  assert(!k_spinlock_holding(&mbox->lock));
  assert(k_list_is_empty(&mbox->receive_list));
  assert(k_list_is_empty(&mbox->send_list));
}

static void
k_mailbox_init_common(struct KMailBox *mbox, size_t msg_size, void *start,
                      size_t size)
{
  mbox->buf_start = (uint8_t *) start;
  mbox->buf_end   = mbox->buf_start + ROUND_DOWN(size, msg_size);
  mbox->read_ptr  = mbox->buf_start;
  mbox->write_ptr = mbox->buf_start;
  mbox->msg_size  = msg_size;
  mbox->max_size  = size / msg_size;
  mbox->size      = 0;
}
