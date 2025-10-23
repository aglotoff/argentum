#ifndef __INCLUDE_KERNEL_CORE_MAILBOX_H__
#define __INCLUDE_KERNEL_CORE_MAILBOX_H__

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

/**
 * @brief Represents a kernel mailbox object.
 *
 * A mailbox provides synchronized message-passing between tasks using a
 * circular buffer. It supports multiple concurrent senders and receivers.
 */
struct KMailBox {
  int type;
  int flags;
  struct KSpinLock lock;
  k_uint8_t *buf_start;
  k_uint8_t *buf_end;
  k_uint8_t *read_ptr;
  k_uint8_t *write_ptr;
  k_size_t size;
  k_size_t capacity;
  k_size_t msg_size;
  struct KListLink receivers;
  struct KListLink senders;
};

int k_mailbox_create(struct KMailBox *, k_size_t, void *, k_size_t);
void k_mailbox_destroy(struct KMailBox *);
int k_mailbox_try_receive(struct KMailBox *, void *);
int k_mailbox_timed_receive(struct KMailBox *, void *, k_tick_t, int);
int k_mailbox_try_send(struct KMailBox *, const void *);
int k_mailbox_timed_send(struct KMailBox *, const void *, k_tick_t, int);

/**
 * @brief Receive a message from a mailbox.
 *
 * Attempts to receive a message from the specified mailbox. If no message is
 * currently available, the calling task may block depending on the mailbox's
 * configuration and the specified options.
 *
 * @param mailbox Pointer to the mailbox to receive from.
 * @param message Pointer to the destination buffer where the received message
 *                will be copied.
 * @param options Bitmask of receive options controlling blocking behavior.
 *
 * @return
 * - `0` on success.
 * - `K_ERR_AGAIN` if the mailbox is empty and the operation would block.
 * - `K_ERR_INVAL` if the mailbox is invalid or destroyed.
 *
 * @note This function may cause the calling task to sleep if no message is
 *       available and blocking is permitted by `options`.
 */
static inline int
k_mailbox_receive(struct KMailBox *mailbox, void *message, int options)
{
  return k_mailbox_timed_receive(mailbox, message, 0, options);
}

/**
 * @brief Send a message to a mailbox.
 *
 * Attempts to send a message to the specified mailbox. If the mailbox is full,
 * the calling task may block depending on the mailbox's configuration and the
 * specified options.
 *
 * @param mailbox Pointer to the mailbox to send to.
 * @param message Pointer to the message data to be sent. The message size must
 *                match the mailbox's configured message size.
 * @param options Bitmask of send options controlling blocking behavior.
 *
 * @return
 * - `0` on success.
 * - `K_ERR_AGAIN` if the mailbox is full and the operation would block.
 * - `K_ERR_INVAL` if the mailbox is invalid or destroyed.
 *
 * @note This function may cause the calling task to sleep if the mailbox is
 *       full and blocking is permitted by `options`.
 */
static inline int
k_mailbox_send(struct KMailBox *mailbox, const void *message, int options)
{
  return k_mailbox_timed_send(mailbox, message, 0, options);
}

#endif  // !__INCLUDE_KERNEL_CORE_MAILBOX_H__
