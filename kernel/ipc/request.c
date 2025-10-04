#include <kernel/ipc.h>
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

    if (vm_space_copy_out(req->process,
                          msg,
                          (uintptr_t)iov->iov_base + req->recv_pos,
                          nwrite) < 0)
      k_panic("copy");

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

    if (vm_space_copy_in(req->process,
                         msg,
                         (uintptr_t)iov->iov_base + req->send_pos,
                         nread) < 0)
      k_panic("copy");

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
