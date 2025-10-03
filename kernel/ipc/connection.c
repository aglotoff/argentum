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

intptr_t
ipc_send(struct Connection *connection, void *smsg, size_t sbytes, void *rmsg, size_t rbytes)
{
  k_assert(connection->ref_count > 0);

  // if (process_current()->pid > 3)
  //   cprintf("[k] proc #%x: %d -> %d\n", process_current()->pid, *(int *) smsg, connection->type);

  switch (connection->type) {
    case CONNECTION_TYPE_FILE:
      return connection_send(connection, smsg, sbytes, rmsg, rbytes);
    case CONNECTION_TYPE_PIPE:
      return pipe_send_recv(connection, smsg, sbytes, rmsg, rbytes);
    case CONNECTION_TYPE_SOCKET:
      return net_send_recv(connection, smsg, sbytes, rmsg, rbytes);
    default:
      k_panic("bad connection type %d", connection->type);
      return -ENOSYS;
  }
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
    ipc_send(connection, &msg, sizeof(msg), NULL, 0);

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

  return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
}

ssize_t
connection_read(struct Connection *connection, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_READ;
  msg.u.read.va    = va;
  msg.u.read.nbyte = nbytes;

  return ipc_send(connection, &msg, sizeof(msg), (void *) va, nbytes);
}

ssize_t
connection_write(struct Connection *connection, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_WRITE;
  msg.u.write.va    = va;
  msg.u.write.nbyte = nbytes;

  return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
}

ssize_t
connection_getdents(struct Connection *connection, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  if ((connection->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  msg.type = IPC_MSG_READDIR;
  msg.u.readdir.va    = va;
  msg.u.readdir.nbyte = nbytes;
  
  return ipc_send(connection, &msg, sizeof(msg), (void *) va, nbytes);
}

int
connection_stat(struct Connection *connection, struct stat *buf)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSTAT;
  msg.u.fstat.buf  = buf;

  return ipc_send(connection, &msg, sizeof(msg), buf, sizeof(*buf));
}

int
connection_chdir(struct Connection *connection)
{
  if (connection->node == NULL)
    return -ENOTDIR;

  return fs_path_set_cwd(connection->node);
}

int
connection_chmod(struct Connection *connection, mode_t mode)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FCHMOD;
  msg.u.fchmod.mode = mode;

  return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
}

int
connection_chown(struct Connection *connection, uid_t uid, gid_t gid)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FCHOWN;
  msg.u.fchown.uid  = uid;
  msg.u.fchown.gid  = gid;

  return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
}

int
connection_ioctl(struct Connection *connection, int request, int arg)
{
  struct IpcMessage msg;
  
  msg.type = IPC_MSG_IOCTL;
  msg.u.ioctl.request = request;
  msg.u.ioctl.arg     = arg;

  switch (request) {
  case TIOCGETA:
    return ipc_send(connection, &msg, sizeof(msg), (void *) arg, sizeof(struct termios));
  case TIOCGWINSZ:
    return ipc_send(connection, &msg, sizeof(msg), (void *) arg, sizeof(struct winsize));
  default:
    return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
  }
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
connection_truncate(struct Connection *connection, off_t length)
{
  struct IpcMessage msg;

  if ((connection->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  k_assert(connection->ref_count > 0);
  k_assert(connection->type == CONNECTION_TYPE_FILE);

  msg.type = IPC_MSG_TRUNC;
  msg.u.trunc.length = length;

  return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
}

int
connection_sync(struct Connection *connection)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSYNC;

  return ipc_send(connection, &msg, sizeof(msg), NULL, 0);
}

intptr_t
connection_send(struct Connection *connection, void *smsg, size_t sbytes, void *rmsg, size_t rbytes)
{
  struct Request *req;
  unsigned long long timeout = seconds2ticks(15);
  intptr_t r;

  if (connection->endpoint == NULL)
    return -1;

  if ((req = request_create()) == NULL)
    return -ENOMEM;  

  req->send_msg = (uintptr_t) smsg;
  req->send_bytes = sbytes;
  req->recv_msg = (uintptr_t) rmsg;
  req->recv_bytes = rbytes;

  req->connection = connection;
  req->process = process_current();

  request_dup(req);

  if (k_mailbox_timed_send(&connection->endpoint->mbox, &req, timeout) < 0) {
    request_destroy(req);
    request_destroy(req);
    k_panic("fail send:\n");
    return -ETIMEDOUT;
  }

  if ((r = k_semaphore_timed_get(&req->sem, timeout, 0)) < 0) {
    request_destroy(req);
    request_destroy(req);
    k_panic("fail recv:\n");
    return -ETIMEDOUT;
  }

  r = req->r;

  request_destroy(req);

  return r;
}
