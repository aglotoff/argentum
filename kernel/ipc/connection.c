#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>

#include <kernel/core/assert.h>
#include <kernel/console.h>
#include <kernel/ipc.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/core/spinlock.h>
#include <kernel/net.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/time.h>

static struct KSpinLock connection_lock;
static struct KObjectPool *connection_pool;

void
connection_init(void)
{
  if (!(connection_pool = k_object_pool_create("connection_pool", sizeof(struct Connection), 0, NULL, NULL)))
    k_panic("Cannot allocate connection pool");

  k_spinlock_init(&connection_lock, "connection_lock");
}

int
connection_alloc(struct Connection **fstore)
{
  struct Connection *f;

  if ((f = (struct Connection *) k_object_pool_get(connection_pool)) == NULL)
    return -ENOMEM;

  f->type      = 0;
  f->ref_count = 0;
  f->flags     = 0;
  f->node      = NULL;
  f->endpoint  = NULL;

  if (fstore != NULL)
    *fstore = f;

  return 0;
}

struct Connection *
connection_ref(struct Connection *connection)
{
  k_spinlock_acquire(&connection_lock);
  connection->ref_count++;
  k_spinlock_release(&connection_lock);

  return connection;
}

// FIXME: this is a random guess
#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC | O_RDONLY | O_RDWR | O_WRONLY)

int
connection_get_flags(struct Connection *connection)
{
  int r;

  k_spinlock_acquire(&connection_lock);
  r = connection->flags & STATUS_MASK;
  k_spinlock_release(&connection_lock);

  return r;
}

int
connection_set_flags(struct Connection *connection, int flags)
{
  k_spinlock_acquire(&connection_lock);
  connection->flags = (connection->flags & ~STATUS_MASK) | (flags & STATUS_MASK);
  k_spinlock_release(&connection_lock);
  return 0;
}

void
connection_unref(struct Connection *connection)
{
  int ref_count;
  
  k_spinlock_acquire(&connection_lock);

  if (connection->ref_count < 1)
    k_panic("bad ref_count %d", ref_count);

  if (connection->ref_count == 1) {
    struct IpcMessage msg;

    k_spinlock_release(&connection_lock);

    msg.type = IPC_MSG_CLOSE;
    connection_send(connection, &msg, sizeof(msg), NULL, 0);

    k_spinlock_acquire(&connection_lock);
  };

  ref_count = --connection->ref_count;

  k_spinlock_release(&connection_lock);

  if (ref_count > 0)
    return;

  if (connection->node != NULL) {
    fs_path_node_unref(connection->node);
    connection->node = NULL;
  }

  k_object_pool_put(connection_pool, connection);
}

off_t
connection_seek(struct Connection *connection, off_t offset, int whence)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_SEEK;
  msg.u.seek.offset = offset;
  msg.u.seek.whence = whence;

  return connection_send(connection, &msg, sizeof(msg), NULL, 0);
}

ssize_t
connection_read(struct Connection *connection, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_READ;
  msg.u.read.nbyte = nbytes;

  return connection_send(connection, &msg, sizeof(msg), (void *) va, nbytes);
}

int
connection_stat(struct Connection *connection, struct stat *buf)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSTAT;

  return connection_send(connection, &msg, sizeof(msg), buf, sizeof(*buf));
}

int
connection_chdir(struct Connection *connection)
{
  if (connection->node == NULL)
    return -ENOTDIR;

  return fs_path_set_cwd(connection->node);
}

// TODO
int
connection_select(struct Connection *connection, struct timeval *timeout)
{
  switch (connection->type) {
  case CONNECTION_TYPE_FILE:
    return fs_select(connection, timeout);
  case CONNECTION_TYPE_SOCKET:
    return net_select(connection, timeout);
  case CONNECTION_TYPE_PIPE:
    return pipe_select(connection, timeout);
  default:
    k_panic("bad connection type %d", connection->type);
    return -EBADF;
  }
}

int
connection_sync(struct Connection *connection)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSYNC;

  return connection_send(connection, &msg, sizeof(msg), NULL, 0);
}

intptr_t
connection_send(struct Connection *connection, void *smsg, size_t sbytes, void *rmsg, size_t rbytes)
{
  struct Request *req;
  unsigned long long timeout = seconds2ticks(15);
  intptr_t r;

  //cprintf("[k] proc %x sends %d to %p\n", process_current()->pid, *(int *) smsg, connection->endpoint);

  if (connection->endpoint == NULL)
    return -1;

  if ((req = request_create()) == NULL)
    return -ENOMEM;  

  if (smsg && sbytes) {
    if ((req->send_iov = k_malloc(sizeof(struct iovec))) == NULL)
      k_panic("out of memory");
    
    req->send_iov->iov_base = smsg;
    req->send_iov->iov_len  = sbytes;
    req->send_iov_cnt       = 1;
  }

  if (rmsg && rbytes) {
    if ((req->recv_iov = k_malloc(sizeof(struct iovec))) == NULL)
      k_panic("out of memory");

    req->recv_iov->iov_base = rmsg;
    req->recv_iov->iov_len  = rbytes;
    req->recv_iov_cnt       = 1;
  }

  req->connection = connection;
  req->process = process_current();

  request_dup(req);

  if (k_mailbox_timed_send(&connection->endpoint->mbox, &req, timeout) < 0) {
    k_panic("fail send:\n");
    request_destroy(req);
    request_destroy(req);
    return -ETIMEDOUT;
  }

  // if (connection->type == CONNECTION_TYPE_PIPE) {
  //   cprintf("[k] proc #%x sent to %p\n", process_current()->pid, connection->endpoint);
  // }

  if ((r = k_semaphore_timed_get(&req->sem, timeout, K_SLEEP_UNINTERUPTIBLE)) < 0) {
    k_panic("fail recv:\n");
    request_destroy(req);
    request_destroy(req);
    return -ETIMEDOUT;
  }

  r = req->r;

  request_destroy(req);

  return r;
}


intptr_t
connection_sendv(struct Connection *connection,
                 struct iovec *send_iov, int send_iov_cnt,
                 struct iovec *recv_iov, int recv_iov_cnt)
{
  struct Request *req;
  unsigned long long timeout = seconds2ticks(15);
  intptr_t r;

  //cprintf("[k] proc %x sends %d to %p\n", process_current()->pid, *(int *) smsg, connection->endpoint);

  if (connection->endpoint == NULL)
    return -1;

  if ((req = request_create()) == NULL)
    return -ENOMEM;  

  if (send_iov && send_iov_cnt) {
    if ((req->send_iov = k_malloc(sizeof(struct iovec) * send_iov_cnt)) == NULL)
      k_panic("out of memory");
    memmove(req->send_iov, send_iov, sizeof(struct iovec) * send_iov_cnt);
    req->send_iov_cnt = send_iov_cnt;
  }

  if (recv_iov && recv_iov_cnt) {
    if ((req->recv_iov = k_malloc(sizeof(struct iovec) * recv_iov_cnt)) == NULL)
      k_panic("out of memory");
    memmove(req->recv_iov, recv_iov, sizeof(struct iovec) * recv_iov_cnt);
    req->recv_iov_cnt = recv_iov_cnt;
  }

  req->connection = connection;
  req->process = process_current();

  request_dup(req);

  if (k_mailbox_timed_send(&connection->endpoint->mbox, &req, timeout) < 0) {
    k_panic("fail send:\n");
    request_destroy(req);
    request_destroy(req);
    return -ETIMEDOUT;
  }

  if ((r = k_semaphore_timed_get(&req->sem, timeout, K_SLEEP_UNINTERUPTIBLE)) < 0) {
    k_panic("fail recv:\n");
    request_destroy(req);
    request_destroy(req);
    return -ETIMEDOUT;
  }

  r = req->r;

  request_destroy(req);

  return r;
}
