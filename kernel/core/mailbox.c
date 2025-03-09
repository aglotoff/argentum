#include <kernel/core/cpu.h>
#include <kernel/core/mailbox.h>
#include <kernel/core/task.h>

#include "core_private.h"

static int  k_mailbox_try_receive_locked(struct KMailBox *, void *);
static int  k_mailbox_try_send_locked(struct KMailBox *, const void *);

int
k_mailbox_create(struct KMailBox *mailbox,
                 size_t msg_size,
                 void *buf,
                 size_t buf_size)
{
  k_spinlock_init(&mailbox->lock, "k_mailbox");
  k_list_init(&mailbox->receivers);
  k_list_init(&mailbox->senders);
  mailbox->type = K_MAILBOX_TYPE;
  mailbox->buf_start = (uint8_t *) buf;
  mailbox->buf_end   = mailbox->buf_start + buf_size - (buf_size % msg_size);
  mailbox->read_ptr  = mailbox->buf_start;
  mailbox->write_ptr = mailbox->buf_start;
  mailbox->msg_size  = msg_size;
  mailbox->capacity  = buf_size / msg_size;
  mailbox->size      = 0;
  mailbox->flags     = 0;
  return 0;
}

int
k_mailbox_destroy(struct KMailBox *mailbox)
{
  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);

  _k_sched_wakeup_all(&mailbox->receivers, K_ERR_INVAL);
  _k_sched_wakeup_all(&mailbox->senders, K_ERR_INVAL);

  k_spinlock_release(&mailbox->lock);

  return 0;
}

int
k_mailbox_try_receive(struct KMailBox *mailbox, void *message)
{
  int r;

  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);
  r = k_mailbox_try_receive_locked(mailbox, message);
  k_spinlock_release(&mailbox->lock);

  return r;
}

int
k_mailbox_timed_receive(struct KMailBox *mailbox,
                        void *message,
                        unsigned long timeout)
{
  int r;

  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);

  while ((r = k_mailbox_try_receive_locked(mailbox, message)) != 0) {
    if (r != K_ERR_AGAIN)
      break;

    r = _k_sched_sleep(&mailbox->receivers,
                       K_TASK_STATE_SLEEP,
                       timeout,
                       &mailbox->lock);
    if (r < 0)
      break;
  }

  k_spinlock_release(&mailbox->lock);

  return r;
}

static int
k_mailbox_try_receive_locked(struct KMailBox *mailbox, void *message)
{
  k_assert(k_spinlock_holding(&mailbox->lock));

  if (mailbox->size == 0)
    return K_ERR_AGAIN;

  k_memmove(message, mailbox->read_ptr, mailbox->msg_size);

  mailbox->read_ptr += mailbox->msg_size;
  if (mailbox->read_ptr >= mailbox->buf_end)
    mailbox->read_ptr = mailbox->buf_start;

  if (mailbox->size-- == mailbox->capacity)
    _k_sched_wakeup_one(&mailbox->senders, 0);

  return 0;
}

int
k_mailbox_try_send(struct KMailBox *mailbox, const void *message)
{
  int r;

  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);
  r = k_mailbox_try_send_locked(mailbox, message);
  k_spinlock_release(&mailbox->lock);

  return r;
}

int
k_mailbox_timed_send(struct KMailBox *mailbox,
                     const void *message,
                     unsigned long timeout)
{
  int r;

  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);

  while ((r = k_mailbox_try_send_locked(mailbox, message)) != 0) {
    if (r != K_ERR_AGAIN)
      break;

    r = _k_sched_sleep(&mailbox->senders,
                       K_TASK_STATE_SLEEP,
                       timeout,
                       &mailbox->lock);
    if (r < 0)
      break;
  }

  k_spinlock_release(&mailbox->lock);

  return r;
}

static int
k_mailbox_try_send_locked(struct KMailBox *mailbox, const void *message)
{
  k_assert(k_spinlock_holding(&mailbox->lock));
  
  if (mailbox->size == mailbox->capacity)
    return K_ERR_AGAIN;
  
  k_memmove(mailbox->write_ptr, message, mailbox->msg_size);

  mailbox->write_ptr += mailbox->msg_size;
  if (mailbox->write_ptr >= mailbox->buf_end)
    mailbox->write_ptr = mailbox->buf_start;

  if (mailbox->size++ == 0)
    _k_sched_wakeup_one(&mailbox->receivers, 0);

  return 0;
}
