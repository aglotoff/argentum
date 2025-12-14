#include <kernel/ipc.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/object_pool.h>

struct Request *
request_create(void)
{
  struct Request *req;

  if ((req = k_malloc(sizeof *req)) == NULL)
    return NULL;

  k_semaphore_create(&req->sem, 0);
  k_spinlock_init(&req->lock, "req");
  
  req->process = NULL;
  req->connection = NULL;

  req->send_iov = NULL;
  req->send_iov_cnt = 0;
  req->send_idx = 0;
  req->send_pos = 0;

  req->recv_iov = NULL;
  req->recv_iov_cnt = 0;
  req->recv_idx = 0;
  req->recv_pos = 0;
  
  req->ref_count = 1;

  return req;
}

void
request_destroy(struct Request *req)
{
  int count;

  k_assert(req->ref_count > 0);

  k_spinlock_acquire(&req->lock);

  req->connection = NULL;
  req->process = NULL;

  count = --req->ref_count;

  k_spinlock_release(&req->lock);

  if (count == 0) {
    if (req->send_iov != NULL)
      k_free(req->send_iov);
    if (req->recv_iov != NULL)
      k_free(req->recv_iov);

    k_free(req);
  }
}

void
request_dup(struct Request *req)
{
  k_spinlock_acquire(&req->lock);
  req->ref_count++;
  k_spinlock_release(&req->lock);
}


void
request_reply(struct Request *req, intptr_t r)
{
  req->r = r;

  k_semaphore_put(&req->sem);
  request_destroy(req);
}

int
vm_space_copy_in(struct Process *process, void *dst, uintptr_t src_va, size_t n)
{
  if ((src_va + n) < src_va)
    return -EFAULT;

  if (src_va > VIRT_KERNEL_BASE) {
    // TODO: check user permissions
    memmove(dst, (void *) src_va, n);
    return 0;
  }

  k_assert(process != NULL);

  return vm_user_copy_in(process->vm->pgtab, dst, src_va, n);
}

ssize_t
request_write(struct Request *req, void *msg, size_t n)
{
  size_t total_written = 0;

  if (req->recv_iov == NULL)
    return total_written;

  while ((n > 0) && (req->recv_idx < req->recv_iov_cnt)) {
    struct iovec *iov = &req->recv_iov[req->recv_idx];

    size_t iov_remaining = iov->iov_len - req->recv_pos;
    size_t nwrite = MIN(iov_remaining, n);

    if (nwrite == 0) {
      // Move to the next iovec if the current one is full
      req->recv_idx++;
      req->recv_pos = 0;
      continue;
    }

    uintptr_t dst_va = (uintptr_t) iov->iov_base + req->recv_pos;

    if ((dst_va + n) < dst_va)
      return -EFAULT;

    if (dst_va > VIRT_KERNEL_BASE) {
      // TODO: check user permissions
      memmove((void *) dst_va, msg, n);
    } else {
      k_assert(req->process != NULL);

      if (vm_user_copy_out(req->process->vm->pgtab, msg, dst_va, n) < 0)
        k_panic("request_write");
    }

    req->recv_pos += nwrite;
    msg = (char *) msg + nwrite;
    n -= nwrite;
    total_written += nwrite;

    if (req->recv_pos == iov->iov_len) {
      req->recv_idx++;
      req->recv_pos = 0;
    }
  }

  return total_written;
}

ssize_t
request_read(struct Request *req, void *msg, size_t n)
{
  size_t total_read = 0;

  if (req->send_iov == NULL)
    return total_read;

  while ((n > 0) && (req->send_idx < req->send_iov_cnt)) {
    struct iovec *iov = &req->send_iov[req->send_idx];

    size_t iov_remaining = iov->iov_len - req->send_pos;
    size_t nread = MIN(iov_remaining, n);

    if (nread == 0) {
      // Move to the next iovec if the current one is full
      req->send_idx++;
      req->send_pos = 0;
      continue;
    }

    uintptr_t src_va = (uintptr_t) iov->iov_base + req->send_pos;

    if ((src_va + n) < src_va)
      return -EFAULT;

    if (src_va > VIRT_KERNEL_BASE) {
      // TODO: check user permissions
      memmove(msg, (const void *) src_va, n);
    } else {
      k_assert(req->process != NULL);

      if (vm_user_copy_in(req->process->vm->pgtab, msg, src_va, n) < 0)
        k_panic("request_read");
    }

    req->send_pos += nread;
    msg = (char *) msg + nread;
    n -= nread;
    total_read += nread;

    if (req->send_pos == iov->iov_len) {
      req->send_idx++;
      req->send_pos = 0;
    }
  }

  return total_read;
}
