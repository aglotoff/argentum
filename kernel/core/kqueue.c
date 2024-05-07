#include <errno.h>
#include <string.h>

#include <kernel/cpu.h>
#include <kernel/kqueue.h>
#include <kernel/thread.h>
#include <kernel/types.h>
#include <kernel/cprintf.h>

int
kqueue_init(struct KQueue *queue, size_t msg_size, void *start, size_t size)
{
  queue->buf_start = (uint8_t *) start;
  queue->buf_end   = queue->buf_start + ROUND_DOWN(size, msg_size);
  queue->read_ptr  = queue->buf_start;
  queue->write_ptr = queue->buf_start;
  queue->msg_size  = msg_size;
  queue->max_size  = size / msg_size;
  queue->size      = 0;

  list_init(&queue->receive_list);
  list_init(&queue->send_list);

  return 0;
}

int
kqueue_destroy(struct KQueue *queue)
{
  sched_lock();
  sched_wakeup_all(&queue->receive_list, -EINVAL);
  sched_wakeup_all(&queue->send_list, -EINVAL);
  sched_unlock();

  return 0;
}

int
kqueue_receive(struct KQueue *queue, void *msg, unsigned long timeout,
               int blocking)
{
  struct Thread *my_task = thread_current();
  int r;

  if ((my_task == NULL) && blocking)
    // TODO: choose another value to indicate an error?
    return -EAGAIN;

  sched_lock();

  while (queue->size == 0) {
    struct Cpu *my_cpu = cpu_current();

    if (!blocking || (my_cpu->isr_nesting > 0)) {
      // Can't block
      sched_unlock();
      return -EAGAIN;
    }

    if ((r = sched_sleep(&queue->receive_list, 0, timeout, NULL)) != 0) {
      sched_unlock();
      return r;
    }
  }

  memmove(msg, queue->read_ptr, queue->msg_size);

  queue->read_ptr += queue->msg_size;
  if (queue->read_ptr >= queue->buf_end)
    queue->read_ptr = queue->buf_start;

  if (queue->size-- == queue->max_size)
    sched_wakeup_one(&queue->send_list, 0);

  sched_unlock();

  return r;
}

int
kqueue_send(struct KQueue *queue, const void *msg, unsigned long timeout,
            int blocking)
{
  struct Thread *my_task = thread_current();
  int ret;

  if ((my_task == NULL) && blocking)
    // TODO: choose another value to indicate an error?
    return -EAGAIN;

  sched_lock();

  while (queue->size == queue->max_size) {
    struct Cpu *my_cpu = cpu_current();

    if (!blocking || (my_cpu->isr_nesting > 0)) {
      // Can't block
      sched_unlock();
      return -EAGAIN;
    }

    sched_sleep(&queue->send_list, 0, timeout, NULL);

    if ((ret = my_task->sleep_result) != 0) {
      sched_unlock();
      return ret;
    }
  }

  memmove(queue->write_ptr, msg, queue->msg_size);

  queue->write_ptr += queue->msg_size;
  if (queue->write_ptr >= queue->buf_end)
    queue->write_ptr = queue->buf_start;

  if (queue->size++ == 0)
    sched_wakeup_one(&queue->receive_list, 0);

  sched_unlock();

  return ret;
}
