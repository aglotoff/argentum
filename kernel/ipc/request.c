#include <kernel/ipc.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/object_pool.h>

struct KObjectPool *request_pool;

void request_ctor(void *p, size_t)
{
  struct Request *req = (struct Request *) p;

  k_spinlock_init(&req->lock, "req");
  req->process = NULL;
  req->connection = NULL;
}

void request_dtor(void *p, size_t)
{
  struct Request *req = (struct Request *) p;

  k_assert(req->process == NULL);
  k_assert(req->connection == NULL);
  k_assert(req->ref_count == 0);
}

struct Request *
request_create(void)
{
  struct Request *req;

  if ((req = (struct Request *) k_object_pool_get(request_pool)) == NULL)
    return NULL;

  req->send_idx = 0;
  req->send_pos = 0;

  req->recv_idx = 0;
  req->recv_pos = 0;
  
  req->ref_count = 1;

  k_semaphore_create(&req->sem, 0);

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

  req->recv_iov = NULL;
  req->send_iov = NULL;

  count = --req->ref_count;

  k_spinlock_release(&req->lock);

  if (count == 0) {
    k_semaphore_destroy(&req->sem);
    k_object_pool_put(request_pool, req);
  }
}

void
request_reply(struct Request *req, intptr_t status)
{
  req->status = status;

  k_semaphore_put(&req->sem);
  request_destroy(req);
}

static ssize_t
request_write_locked(struct Request *req, void *msg, size_t n)
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

    if (dst_va > VIRT_KERNEL_BASE) {
      // TODO: check user permissions
      memmove((void *) dst_va, msg, n);
    } else {
      // TODO: may become NULL!
      k_assert(req->process != NULL);

      if (vm_space_copy_out(req->process->vm, msg, dst_va, n) < 0)
        k_panic("request_write");
    }

    msg = (char *) msg + nwrite;
    n -= nwrite;
    total_written += nwrite;

    req->recv_pos += nwrite;

    if (req->recv_pos == iov->iov_len) {
      req->recv_idx++;
      req->recv_pos = 0;
    }
  }

  return total_written;
}

ssize_t
request_write(struct Request *req, void *msg, size_t n)
{
  ssize_t r;

  k_spinlock_acquire(&req->lock);
  r = request_write_locked(req, msg, n);
  k_spinlock_release(&req->lock);

  return r;
}

static ssize_t
request_read_locked(struct Request *req, void *msg, size_t n)
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

    if (src_va > VIRT_KERNEL_BASE) {
      // TODO: check user permissions!
      memmove(msg, (const void *) src_va, n);
    } else {
      // TODO: may become NULL!
      k_assert(req->process != NULL);

      if (vm_space_copy_in(req->process->vm, msg, src_va, n) < 0)
        k_panic("request_read");
    }

    msg = (char *) msg + nread;
    n -= nread;
    total_read += nread;

    req->send_pos += nread;

    if (req->send_pos == iov->iov_len) {
      req->send_idx++;
      req->send_pos = 0;
    }
  }

  return total_read;
}

ssize_t
request_read(struct Request *req, void *msg, size_t n)
{
  ssize_t r;

  k_spinlock_acquire(&req->lock);
  r = request_read_locked(req, msg, n);
  k_spinlock_release(&req->lock);

  return r;
}
