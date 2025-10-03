#include <errno.h>
#include <fcntl.h>

#include <kernel/console.h>
#include <kernel/ipc.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/time.h>
#include <kernel/hash.h>

static struct KObjectPool *pipe_cache;

static const size_t PIPE_BUF_ORDER = 4;
static const size_t PIPE_BUF_SIZE  = (PAGE_SIZE << PIPE_BUF_ORDER);

struct Pipe {
  char           *buf;
  size_t          size;
  size_t          max_size;
  int             read_open;
  int             write_open;
  size_t          read_pos;
  size_t          write_pos;
  struct KMutex   mutex;
  struct KCondVar read_cond;
  struct KCondVar write_cond;
};

struct PipeEndpoint {
  struct KListLink hash_link;
  struct Connection  *connection;
  struct Pipe     *pipe;
  int              flags;
};

typedef void (*pipe_handler_t)(struct Request *, struct IpcMessage *);

static pipe_handler_t
pipe_dispatch_table[] = {
  [IPC_MSG_READ]     = pipe_read,
};

struct Endpoint pipe_endpoint;
struct KTask    pipe_tasks[ENDPOINT_MBOX_CAPACITY];

#define PIPE_DISPATCH_TABLE_SIZE (int)(sizeof(pipe_dispatch_table) / sizeof(pipe_dispatch_table[0]))

void
pipe_service_task(void *)
{
  struct Request *req;

  while (endpoint_receive(&pipe_endpoint, &req) >= 0) {
    struct IpcMessage msg;

    if (request_read(req, &msg, sizeof(msg)) < 0) {
      request_reply(req, -EFAULT);
    } else if (msg.type >= 0 && msg.type < PIPE_DISPATCH_TABLE_SIZE && pipe_dispatch_table[msg.type] != NULL) {
      pipe_dispatch_table[msg.type](req, &msg);
    } else {
      k_warn("unsupported msg type %d\n", msg.type);
      request_reply(req, -ENOSYS);
    }
  }

  k_panic("error");
}



#define NBUCKET   256

static struct {
  struct KListLink table[NBUCKET];
  struct KSpinLock lock;
} pipe_hash;

static struct PipeEndpoint *
pipe_get_connection_endpoint(struct Connection *connection)
{
  struct KListLink *l;

  k_spinlock_acquire(&pipe_hash.lock);

  HASH_FOREACH_ENTRY(pipe_hash.table, l, (uintptr_t) connection) {
    struct PipeEndpoint *endpoint;
    
    endpoint = KLIST_CONTAINER(l, struct PipeEndpoint, hash_link);
    if (endpoint->connection == connection) {
      k_spinlock_release(&pipe_hash.lock);
      return endpoint;
    }
  }

  k_spinlock_release(&pipe_hash.lock);

  k_warn("pipe endpoint not found");

  return NULL;
}

static void
pipe_set_connection_endpoint(struct PipeEndpoint *endpoint,
                          struct Connection *connection,
                          struct Pipe *pipe,
                          int flags)
{
  endpoint->connection = connection;
  endpoint->pipe    = pipe;
  endpoint->flags   = flags;
  k_list_null(&endpoint->hash_link);

  connection->type = CONNECTION_TYPE_PIPE;
  connection->ref_count++;
  connection->endpoint = &pipe_endpoint;

  k_spinlock_acquire(&pipe_hash.lock);
  HASH_PUT(pipe_hash.table, &endpoint->hash_link, (uintptr_t) connection);
  k_spinlock_release(&pipe_hash.lock);
}

void
pipe_init_system(void)
{
  int i;

  pipe_cache = k_object_pool_create("pipe", sizeof(struct Pipe), 0, NULL, NULL);
  if (pipe_cache == NULL)
    k_panic("cannot allocate pipe cache");

  HASH_INIT(pipe_hash.table);
  k_spinlock_init(&pipe_hash.lock, "pipe_hash");

  endpoint_init(&pipe_endpoint);

  for (i = 0; i < ENDPOINT_MBOX_CAPACITY; i++) {
    struct Page *kstack;

    if ((kstack = page_alloc_one(0, 0)) == NULL)
      k_panic("out of memory");

    kstack->ref_count++;

    k_task_create(&pipe_tasks[i], NULL, pipe_service_task, NULL, page2kva(kstack), PAGE_SIZE, 0);
    k_task_resume(&pipe_tasks[i]);
  }
}

static struct Pipe *
pipe_alloc(void)
{
  struct Page *page;
  struct Pipe *pipe;

  if ((pipe = (struct Pipe *) k_object_pool_get(pipe_cache)) == NULL)
    return NULL;

  if ((page = page_alloc_block(PIPE_BUF_ORDER, 0, PAGE_TAG_PIPE)) == NULL) {
    k_object_pool_put(pipe_cache, pipe);
    return NULL;
  }
  
  pipe->buf = (char *) page2kva(page);
  page->ref_count++;

  return pipe;
}

static void
pipe_free(struct Pipe *pipe)
{
  struct Page *page = kva2page(pipe->buf);

  page_assert(page, PIPE_BUF_ORDER, PAGE_TAG_PIPE);
  page_dec_ref(page, PIPE_BUF_ORDER);

  k_object_pool_put(pipe_cache, pipe);
}

static void
pipe_create(struct Pipe *pipe)
{
  k_mutex_init(&pipe->mutex, "pipe");
  pipe->read_open  = 1;
  pipe->write_open = 1;
  pipe->read_pos   = 0;
  pipe->write_pos  = 0;
  pipe->size       = 0;
  pipe->max_size   = PIPE_BUF_SIZE;
  k_condvar_create(&pipe->read_cond);
  k_condvar_create(&pipe->write_cond);
}

static void
pipe_destroy(struct Pipe *pipe)
{
  k_mutex_fini(&pipe->mutex);
  k_condvar_destroy(&pipe->read_cond);
  k_condvar_destroy(&pipe->write_cond);
}

int
pipe_open(struct Connection **read_store, struct Connection **write_store)
{
  struct Pipe *pipe;
  struct PipeEndpoint *read_end, *write_end;
  struct Connection *read_connection, *write_connection;
  int r;

  if ((pipe = pipe_alloc()) == NULL) {
    r = -ENOMEM;
    goto fail1;
  }

  if ((r = connection_alloc(&read_connection)) < 0)
    goto fail2;
  if ((r = connection_alloc(&write_connection)) < 0)
    goto fail3;

  if ((read_end = k_malloc(sizeof *read_end)) == NULL) {
    r = -ENOMEM;
    goto fail4;
  }
  if ((write_end = k_malloc(sizeof *write_end)) == NULL) {
    r = -ENOMEM;
    goto fail5;
  }

  pipe_create(pipe);

  pipe_set_connection_endpoint(read_end, read_connection, pipe, O_RDONLY);
  pipe_set_connection_endpoint(write_end, write_connection, pipe, O_WRONLY);

  *read_store = read_connection;
  *write_store = write_connection;

  return 0;

fail5:
  k_free(read_end);
fail4:
  connection_unref(write_connection);
fail3:
  connection_unref(read_connection);
fail2:
  pipe_free(pipe);
fail1:
  return r;
}

intptr_t
pipe_send_recv(struct Connection *connection, void *smsg, size_t sbytes, void *rmsg, size_t rbytes)
{
  struct IpcMessage *msg = (struct IpcMessage *) smsg;

  switch (msg->type) {
  case IPC_MSG_CLOSE:
    return pipe_close(connection);
  case IPC_MSG_SEEK:
    return -ESPIPE;
  case IPC_MSG_FCHMOD:
    return -EBADF;
  case IPC_MSG_READ:
    return connection_send(connection, smsg, sbytes, rmsg, rbytes);
  case IPC_MSG_WRITE:
    return pipe_write(connection, msg->u.write.va, msg->u.write.nbyte);
  case IPC_MSG_READDIR:
    return -ENOTDIR;
  case IPC_MSG_FSTAT:
    return pipe_stat(connection, (uintptr_t) rmsg);
  case IPC_MSG_FCHOWN:
    return -EBADF;
  case IPC_MSG_IOCTL:
    return -EBADF;
  case IPC_MSG_TRUNC:
    return -EBADF;
  case IPC_MSG_FSYNC:
    return -EBADF;
  default:
    return -ENOSYS;
  }

  return 0;
}

int
pipe_close(struct Connection *connection)
{
  struct PipeEndpoint *endpoint = pipe_get_connection_endpoint(connection);
  struct Pipe *pipe = endpoint->pipe;
  int write = (endpoint->flags & O_ACCMODE) != O_RDONLY;
  int r;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  if (write) {
    pipe->write_open = 0;
    if (pipe->read_open) {
      k_condvar_broadcast(&pipe->read_cond);
    }
  } else {
    pipe->read_open = 0;
    if (pipe->write_open) {
      k_condvar_broadcast(&pipe->write_cond);
    }
  }

  k_spinlock_acquire(&pipe_hash.lock);
  k_list_remove(&endpoint->hash_link);
  k_spinlock_release(&pipe_hash.lock);

  if (pipe->read_open || pipe->write_open) {
    k_mutex_unlock(&pipe->mutex);
    return 0;
  }

  k_mutex_unlock(&pipe->mutex);

  pipe_destroy(pipe);
  pipe_free(pipe);

  return 0;
}

int
pipe_select(struct Connection *connection, struct timeval *timeout)
{
  struct PipeEndpoint *endpoint = pipe_get_connection_endpoint(connection);
  struct Pipe *pipe = endpoint->pipe;
  int r;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  while (pipe->size == 0) {
    unsigned long t = 0;

    if (timeout != NULL) {
      t = timeval2ticks(timeout);
      k_mutex_unlock(&pipe->mutex);
      return 0;
    }

    if ((r = k_condvar_timed_wait(&pipe->read_cond, &pipe->mutex, t)) < 0) {
      k_mutex_unlock(&pipe->mutex);
      return r;
    }
  }

  k_mutex_unlock(&pipe->mutex);

  return 1;
}

void
pipe_read(struct Request *req, struct IpcMessage *msg)
{
  size_t i;
  struct Connection *connection = req->connection;
  struct PipeEndpoint *endpoint = pipe_get_connection_endpoint(connection);
  struct Pipe *pipe = endpoint->pipe;
  size_t n = msg->u.read.nbyte;
  int r;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0) {
    request_reply(req, r);
    return;
  }

  page_assert(kva2page(pipe->buf), PIPE_BUF_ORDER, PAGE_TAG_PIPE);

  while (pipe->write_open && (pipe->size == 0)) {
    int r;

    if ((r = k_condvar_wait(&pipe->read_cond, &pipe->mutex)) < 0) {
      k_mutex_unlock(&pipe->mutex);
      request_reply(req, r);
      return;
    }
  }
  
  // TODO: could copy all available data in one or two steps

  for (i = 0; (i < n) && (pipe->size > 0); ) {
    size_t nread;
    int r;

    nread = MIN(pipe->size, n - i);
    if (pipe->read_pos + nread > pipe->max_size)
      // Prevent overflow
      nread = pipe->max_size - pipe->read_pos;

    r = request_write(req, &pipe->buf[pipe->read_pos], nread);
    if (r < 0) {
      k_condvar_broadcast(&pipe->write_cond);
      k_mutex_unlock(&pipe->mutex);

      request_reply(req, r);
      return;
    }

    pipe->read_pos += nread;
    i += nread;
    pipe->size -= nread;

    if (pipe->read_pos == pipe->max_size)
      pipe->read_pos = 0;
  }

  k_condvar_broadcast(&pipe->write_cond);
  k_mutex_unlock(&pipe->mutex);

  request_reply(req, i);
}

ssize_t
pipe_write(struct Connection *connection, uintptr_t va, size_t n)
{
  size_t i;
  struct PipeEndpoint *endpoint = pipe_get_connection_endpoint(connection);
  struct Pipe *pipe = endpoint->pipe;
  int r;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  page_assert(kva2page(pipe->buf), PIPE_BUF_ORDER, PAGE_TAG_PIPE);

  for (i = 0; i < n; ) {
    size_t nwrite;
    int r;

    while (pipe->read_open && (pipe->size == pipe->max_size)) {
      if ((r = k_condvar_wait(&pipe->write_cond, &pipe->mutex)) < 0) {
        k_mutex_unlock(&pipe->mutex);
        return r;
      }
    }

    nwrite = MIN(pipe->max_size - pipe->size, n - i);
    if (pipe->write_pos + nwrite > pipe->max_size)
      // Prevent overflow
      nwrite = pipe->max_size - pipe->write_pos;

    r = vm_space_copy_in(process_current(), &pipe->buf[pipe->write_pos], va + i, nwrite);
    if (r < 0) {
      k_mutex_unlock(&pipe->mutex);
      return r;
    }

    if (pipe->size == 0)
      k_condvar_broadcast(&pipe->read_cond);

    i += nwrite;
    pipe->size += nwrite;
    pipe->write_pos += nwrite;

    if (pipe->write_pos == pipe->max_size)
      pipe->write_pos = 0;
  }

  k_mutex_unlock(&pipe->mutex);

  return i;
}

int
pipe_stat(struct Connection *connection, uintptr_t va)
{
  struct PipeEndpoint *endpoint = pipe_get_connection_endpoint(connection);
  struct Pipe *pipe = endpoint->pipe;
  struct stat buf;
  int r;

  memset(&buf, 0, sizeof(buf));

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  // TODO: use meaningful values
  buf.st_dev          = 255;
  buf.st_ino          = 0;
  buf.st_mode         = _IFIFO;
  buf.st_nlink        = 0;
  buf.st_uid          = 0;
  buf.st_gid          = 0;
  buf.st_rdev         = 0;
  buf.st_size         = 0;
  buf.st_atim.tv_sec  = 0;
  buf.st_atim.tv_nsec = 0;
  buf.st_mtim.tv_sec  = 0;
  buf.st_mtim.tv_nsec = 0;
  buf.st_ctim.tv_sec  = 0;
  buf.st_ctim.tv_nsec = 0;
  buf.st_blocks       = 0;
  buf.st_blksize      = 0;

  k_mutex_unlock(&pipe->mutex);

  if ((r = vm_space_copy_out(process_current(), &buf, va, sizeof(buf))) < 0)
    return r;

  return 0;
}
