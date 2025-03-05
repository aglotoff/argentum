#include <kernel/core/cpu.h>
#include <kernel/core/mailbox.h>
#include <kernel/core/task.h>
#include <kernel/object_pool.h>

#include "core_private.h"

static void k_mailbox_ctor(void *, size_t);
static void k_mailbox_dtor(void *, size_t);
static void k_mailbox_init_common(struct KMailBox *, size_t, void *, size_t);
static void k_mailbox_fini_common(struct KMailBox *);
static int  k_mailbox_try_receive_locked(struct KMailBox *, void *);
static int  k_mailbox_try_send_locked(struct KMailBox *, const void *);

static struct KObjectPool *k_mailbox_pool;

void
k_mailbox_system_init(void)
{
  k_mailbox_pool = k_object_pool_create("k_mailbox",
                                        sizeof(struct KMailBox),
                                        0,
                                        k_mailbox_ctor,
                                        k_mailbox_dtor);
  if (k_mailbox_pool == NULL)
    k_panic("cannot create mailbox pool");
}

struct KMailBox *
k_mailbox_create(size_t msg_size, size_t buf_size)
{
  struct KMailBox *mailbox;
  void *buf;
  
  if ((mailbox = (struct KMailBox *) k_object_pool_get(k_mailbox_pool)) == NULL)
    return NULL;
  
  if ((buf = k_malloc(buf_size)) == NULL) {
    k_object_pool_put(k_mailbox_pool, mailbox);
    return NULL;
  }

  k_mailbox_init_common(mailbox, msg_size, buf, buf_size);
  mailbox->flags = 0;

  return mailbox;
}

int
k_mailbox_init(struct KMailBox *mailbox,
               size_t msg_size,
               void *buf,
               size_t buf_size)
{
  k_mailbox_ctor(mailbox, sizeof(struct KMailBox));
  k_mailbox_init_common(mailbox, msg_size, buf, buf_size);
  mailbox->flags = K_MAILBOX_STATIC;

  return 0;
}

static void
k_mailbox_init_common(struct KMailBox *mailbox, size_t msg_size, void *start,
                      size_t buf_size)
{
  mailbox->buf_start = (uint8_t *) start;
  mailbox->buf_end   = mailbox->buf_start + buf_size - (buf_size % msg_size);
  mailbox->read_ptr  = mailbox->buf_start;
  mailbox->write_ptr = mailbox->buf_start;
  mailbox->msg_size  = msg_size;
  mailbox->capacity  = buf_size / msg_size;
  mailbox->size      = 0;
}

void
k_mailbox_destroy(struct KMailBox *mailbox)
{
  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);
  k_assert(!(mailbox->flags & K_MAILBOX_STATIC));

  k_spinlock_acquire(&mailbox->lock);

  k_mailbox_fini_common(mailbox);
  k_free(mailbox->buf_start);

  k_spinlock_release(&mailbox->lock);

  k_object_pool_put(k_mailbox_pool, mailbox);
}

int
k_mailbox_fini(struct KMailBox *mailbox)
{
  k_assert(mailbox != NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);
  k_assert(mailbox->flags & K_MAILBOX_STATIC);

  k_spinlock_acquire(&mailbox->lock);
  k_mailbox_fini_common(mailbox);
  k_spinlock_release(&mailbox->lock);

  return 0;
}

static void
k_mailbox_fini_common(struct KMailBox *mailbox)
{
  k_assert(k_spinlock_holding(&mailbox->lock));

  _k_sched_wakeup_all(&mailbox->receivers, K_ERR_INVAL);
  _k_sched_wakeup_all(&mailbox->senders, K_ERR_INVAL);
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

static void
k_mailbox_ctor(void *p, size_t n)
{
  struct KMailBox *mailbox = (struct KMailBox *) p;
  (void) n;

  k_spinlock_init(&mailbox->lock, "k_mailbox");
  k_list_init(&mailbox->receivers);
  k_list_init(&mailbox->senders);
  mailbox->type = K_MAILBOX_TYPE;
}

static void
k_mailbox_dtor(void *p, size_t n)
{
  struct KMailBox *mailbox = (struct KMailBox *) p;
  (void) n;

  k_assert(!k_spinlock_holding(&mailbox->lock));
  k_assert(k_list_is_empty(&mailbox->receivers));
  k_assert(k_list_is_empty(&mailbox->senders));
}
