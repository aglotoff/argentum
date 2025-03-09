#ifndef __KERNEL_INCLUDE_KERNEL_CORE_MAILBOX_H__
#define __KERNEL_INCLUDE_KERNEL_CORE_MAILBOX_H__

#include <stdint.h>

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

struct KMailBox {
  int               type;
  int               flags;
  struct KSpinLock  lock;
  uint8_t          *buf_start;
  uint8_t          *buf_end;
  uint8_t          *read_ptr;
  uint8_t          *write_ptr;
  size_t            size;
  size_t            capacity;
  size_t            msg_size;
  struct KListLink  receivers;
  struct KListLink  senders;
};

#define K_MAILBOX_TYPE    0x4D424F58  // {'M','B','O','X'}

int              k_mailbox_create(struct KMailBox *, size_t, void *, size_t);
int              k_mailbox_destroy(struct KMailBox *);
int              k_mailbox_try_receive(struct KMailBox *, void *);
int              k_mailbox_timed_receive(struct KMailBox *, void *, unsigned long);
int              k_mailbox_try_send(struct KMailBox *, const void *);
int              k_mailbox_timed_send(struct KMailBox *, const void *, unsigned long);

static inline int
k_mailbox_receive(struct KMailBox *mailbox, void *message)
{
  return k_mailbox_timed_receive(mailbox, message, 0);
}

static inline int
k_mailbox_send(struct KMailBox *mailbox, const void *message)
{
  return k_mailbox_timed_send(mailbox, message, 0);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_CORE_MAILBOX_H__
