#include <kernel/core/cpu.h>
#include <kernel/core/mailbox.h>
#include <kernel/core/task.h>

#include "core_private.h"

static int k_mailbox_try_receive_locked(struct KMailBox *, void *);
static int k_mailbox_try_send_locked(struct KMailBox *, const void *);

// Used by the kernel to verify that the object is a valid mailbox
#define K_MAILBOX_TYPE    0x4D424F58  // {'M','B','O','X'}

/**
 * @brief Initialize a mailbox.
 *
 * This function sets up a mailbox object with a specified message size and
 * backing buffer. The buffer is treated as a circular queue divided into
 * fixed-size message slots.
 *
 * @param mailbox   Pointer to the mailbox structure to initialize.
 * @param msg_size  Size of each message (in bytes).
 * @param buf       Pointer to the user-supplied buffer for message storage.
 * @param buf_size  Size of the buffer (in bytes). Must be a multiple of msg_size.
 *
 * @return 0 on success.
 *
 * @note The caller is responsible for ensuring that the buffer is properly
 *       aligned and remains valid for the lifetime of the mailbox.
 */
int
k_mailbox_create(struct KMailBox *mailbox,
                 k_size_t msg_size,
                 void *buf,
                 k_size_t buf_size)
{
  k_spinlock_init(&mailbox->lock, "k_mailbox");
  k_list_init(&mailbox->receivers);
  k_list_init(&mailbox->senders);

  mailbox->type = K_MAILBOX_TYPE;
  mailbox->buf_start = (k_uint8_t *) buf;
  mailbox->buf_end = mailbox->buf_start + buf_size - (buf_size % msg_size);
  mailbox->read_ptr = mailbox->buf_start;
  mailbox->write_ptr = mailbox->buf_start;
  mailbox->msg_size = msg_size;
  mailbox->capacity = buf_size / msg_size;
  mailbox->size = 0;
  mailbox->flags = 0;

  return 0;
}

/**
 * @brief Destroy a mailbox and wake up all waiting tasks.
 *
 * This function invalidates a mailbox and forcibly wakes all tasks currently
 * blocked on send or receive operations, returning an error code to them.
 *
 * @param mailbox Pointer to the mailbox to destroy.
 *
 * @note After destruction, the mailbox must not be used again until reinitialized.
 * @note The caller should ensure that no other task is concurrently accessing
 *       the mailbox when this is called.
 */
void
k_mailbox_destroy(struct KMailBox *mailbox)
{
  k_assert(mailbox != K_NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);

  _k_sched_wakeup_all(&mailbox->receivers, K_ERR_INVAL);
  _k_sched_wakeup_all(&mailbox->senders, K_ERR_INVAL);

  k_spinlock_release(&mailbox->lock);

  mailbox->type = 0;

  // TODO: check reschedule
}

/**
 * @brief Attempt to receive a message from a mailbox (non-blocking).
 *
 * Reads one message from the mailbox if available. If the mailbox is empty,
 * the function immediately returns with an error.
 *
 * @param mailbox Pointer to the mailbox to receive from.
 * @param message Pointer to the destination buffer for the message.
 *
 * @return
 * - 0 on success.
 * - K_ERR_AGAIN if the mailbox is empty.
 */
int
k_mailbox_try_receive(struct KMailBox *mailbox, void *message)
{
  int r;

  k_assert(mailbox != K_NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);
  r = k_mailbox_try_receive_locked(mailbox, message);
  k_spinlock_release(&mailbox->lock);

  return r;
}

/**
 * @brief Receive a message from a mailbox with timeout.
 *
 * If no message is available, the calling task will sleep until a message
 * arrives, the timeout expires, or the mailbox is destroyed.
 *
 * @param mailbox Pointer to the mailbox to receive from.
 * @param message Pointer to the destination buffer for the message.
 * @param timeout Maximum time to wait (in ticks).
 * @param options Bitmask of sleep options (e.g., K_SLEEP_UNWAKEABLE).
 *
 * @return
 * - 0 on success.
 * - K_ERR_AGAIN if timeout expired.
 * - K_ERR_INVAL if mailbox was destroyed or invalidated.
 */
int
k_mailbox_timed_receive(struct KMailBox *mailbox,
                        void *message,
                        k_tick_t timeout,
                        int options)
{
  int r;

  k_assert(mailbox != K_NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);

  while ((r = k_mailbox_try_receive_locked(mailbox, message)) != 0) {
    if (r != K_ERR_AGAIN)
      break;

    r = _k_sched_sleep(&mailbox->receivers,
                       options & K_SLEEP_UNWAKEABLE
                        ? K_TASK_STATE_SLEEP_UNWAKEABLE
                        : K_TASK_STATE_SLEEP,
                       timeout,
                       &mailbox->lock);
    if (r < 0)
      break;
  }

  k_spinlock_release(&mailbox->lock);

  // TODO: check reschedule

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

  mailbox->size--;
  _k_sched_wakeup_one(&mailbox->senders, 0);

  return 0;
}

/**
 * @brief Attempt to send a message to a mailbox (non-blocking).
 *
 * Writes one message into the mailbox if space is available. If the mailbox
 * is full, the function immediately returns with an error.
 *
 * @param mailbox Pointer to the mailbox to send to.
 * @param message Pointer to the message data to send.
 *
 * @return
 * - 0 on success.
 * - K_ERR_AGAIN if the mailbox is full.
 */
int
k_mailbox_try_send(struct KMailBox *mailbox, const void *message)
{
  int r;

  k_assert(mailbox != K_NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);
  r = k_mailbox_try_send_locked(mailbox, message);
  k_spinlock_release(&mailbox->lock);

  return r;
}

/**
 * @brief Send a message to a mailbox with timeout.
 *
 * If the mailbox is full, the calling task will sleep until space becomes
 * available, the timeout expires, or the mailbox is destroyed.
 *
 * @param mailbox Pointer to the mailbox to send to.
 * @param message Pointer to the message data to send.
 * @param timeout Maximum time to wait (in ticks).
 * @param options Bitmask of sleep options (e.g., K_SLEEP_UNWAKEABLE).
 *
 * @return
 * - 0 on success.
 * - K_ERR_AGAIN if timeout expired.
 * - K_ERR_INVAL if mailbox was destroyed or invalidated.
 */
int
k_mailbox_timed_send(struct KMailBox *mailbox,
                     const void *message,
                     k_tick_t timeout,
                     int options)
{
  int r;

  k_assert(mailbox != K_NULL);
  k_assert(mailbox->type == K_MAILBOX_TYPE);

  k_spinlock_acquire(&mailbox->lock);

  while ((r = k_mailbox_try_send_locked(mailbox, message)) != 0) {
    if (r != K_ERR_AGAIN)
      break;

    r = _k_sched_sleep(&mailbox->senders,
                       options & K_SLEEP_UNWAKEABLE
                        ? K_TASK_STATE_SLEEP_UNWAKEABLE
                        : K_TASK_STATE_SLEEP,
                       timeout,
                       &mailbox->lock);
    if (r < 0)
      break;
  }

  k_spinlock_release(&mailbox->lock);

  // TODO: check reschedule

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

  mailbox->size++;
  _k_sched_wakeup_one(&mailbox->receivers, 0);

  return 0;
}
