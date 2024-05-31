#ifndef __KERNEL_INCLUDE_KERNEL_k_mailbox_H__
#define __KERNEL_INCLUDE_KERNEL_k_mailbox_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/list.h>
#include <kernel/spin.h>

struct KMailBox {
  uint8_t          *buf_start;
  uint8_t          *buf_end;
  uint8_t          *read_ptr;
  uint8_t          *write_ptr;
  size_t            size;
  size_t            max_size;
  size_t            msg_size;
  struct ListLink   receive_list;
  struct ListLink   send_list;
};

int k_mailbox_init(struct KMailBox *, size_t, void *, size_t);
int k_mailbox_destroy(struct KMailBox *);
int k_mailbox_receive(struct KMailBox *, void *, unsigned long, int);
int k_mailbox_send(struct KMailBox *, const void *, unsigned long, int);

#endif  // !__KERNEL_INCLUDE_KERNEL_k_mailbox_H__
