#ifndef __KERNEL_INCLUDE_KERNEL_KQUEUE_H__
#define __KERNEL_INCLUDE_KERNEL_KQUEUE_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/list.h>
#include <kernel/spinlock.h>

struct KQueue {
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

int kqueue_init(struct KQueue *, size_t, void *, size_t);
int kqueue_destroy(struct KQueue *);
int kqueue_receive(struct KQueue *, void *, unsigned long, int);
int kqueue_send(struct KQueue *, const void *, unsigned long, int);

#endif  // !__KERNEL_INCLUDE_KERNEL_KQUEUE_H__
