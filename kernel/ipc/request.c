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
  req->send_msg = 0;
  req->send_nread = 0;
  req->send_bytes = 0;
  req->recv_msg = 0;
  req->recv_bytes = 0;
  req->recv_nwrite = 0;
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
  size_t nwrite = MIN(req->recv_bytes - req->recv_nwrite, n);

  if (nwrite != 0) {
    if (vm_space_copy_out(req->process, msg, req->recv_msg + req->recv_nwrite, nwrite) < 0)
      k_panic("copy");
    req->recv_nwrite += nwrite;
  }

  return nwrite;
}

ssize_t
request_read(struct Request *req, void *msg, size_t n)
{
  size_t nread = MIN(req->send_bytes - req->send_nread, n);

  if (nread != 0) {
    if (vm_space_copy_in(req->process, msg, req->send_msg + req->send_nread, nread) < 0)
      k_panic("copy");
    req->send_nread += nread;
  }

  return nread;
}
