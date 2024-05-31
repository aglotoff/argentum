#include <errno.h>
#include <string.h>

#include <kernel/cpu.h>
#include <kernel/mailbox.h>
#include <kernel/thread.h>
#include <kernel/types.h>
#include <kernel/cprintf.h>

#include "core_private.h"

int
k_mailbox_init(struct KMailBox *mbox, size_t msg_size, void *start, size_t size)
{
  mbox->buf_start = (uint8_t *) start;
  mbox->buf_end   = mbox->buf_start + ROUND_DOWN(size, msg_size);
  mbox->read_ptr  = mbox->buf_start;
  mbox->write_ptr = mbox->buf_start;
  mbox->msg_size  = msg_size;
  mbox->max_size  = size / msg_size;
  mbox->size      = 0;

  list_init(&mbox->receive_list);
  list_init(&mbox->send_list);

  return 0;
}

int
k_mailbox_destroy(struct KMailBox *mbox)
{
  _k_sched_lock();
  _k_sched_wakeup_all(&mbox->receive_list, -EINVAL);
  _k_sched_wakeup_all(&mbox->send_list, -EINVAL);
  _k_sched_unlock();

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

  _k_sched_lock();

  while (mbox->size == 0) {
    struct KCpu *my_cpu = k_cpu();

    if (!blocking || (my_cpu->isr_nesting > 0)) {
      // Can't block
      _k_sched_unlock();
      return -EAGAIN;
    }

    if ((r = _k_sched_sleep(&mbox->receive_list, 0, timeout, NULL)) != 0) {
      _k_sched_unlock();
      return r;
    }
  }

  memmove(msg, mbox->read_ptr, mbox->msg_size);

  mbox->read_ptr += mbox->msg_size;
  if (mbox->read_ptr >= mbox->buf_end)
    mbox->read_ptr = mbox->buf_start;

  if (mbox->size-- == mbox->max_size)
    _k_sched_wakeup_one(&mbox->send_list, 0);

  _k_sched_unlock();

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

  _k_sched_lock();

  while (mbox->size == mbox->max_size) {
    struct KCpu *my_cpu = k_cpu();

    if (!blocking || (my_cpu->isr_nesting > 0)) {
      // Can't block
      _k_sched_unlock();
      return -EAGAIN;
    }

    _k_sched_sleep(&mbox->send_list, 0, timeout, NULL);

    if ((ret = my_task->sleep_result) != 0) {
      _k_sched_unlock();
      return ret;
    }
  }

  memmove(mbox->write_ptr, msg, mbox->msg_size);

  mbox->write_ptr += mbox->msg_size;
  if (mbox->write_ptr >= mbox->buf_end)
    mbox->write_ptr = mbox->buf_start;

  if (mbox->size++ == 0)
    _k_sched_wakeup_one(&mbox->receive_list, 0);

  _k_sched_unlock();

  return ret;
}
