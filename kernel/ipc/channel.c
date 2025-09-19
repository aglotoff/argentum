#include <kernel/core/assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/console.h>
#include <kernel/ipc/channel.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/core/spinlock.h>
#include <kernel/net.h>
#include <kernel/pipe.h>

static struct KSpinLock channel_lock;
static struct KObjectPool *channel_pool;

void
channel_init(void)
{
  if (!(channel_pool = k_object_pool_create("channel_pool", sizeof(struct Channel), 0, NULL, NULL)))
    k_panic("Cannot allocate channel pool");

  k_spinlock_init(&channel_lock, "channel_lock");
}

int
channel_alloc(struct Channel **fstore)
{
  struct Channel *f;

  if ((f = (struct Channel *) k_object_pool_get(channel_pool)) == NULL)
    return -ENOMEM;

  f->type      = 0;
  f->ref_count = 0;
  f->flags     = 0;
  f->u.file.offset    = 0;
  f->node             = NULL;
  f->u.file.inode     = NULL;
  f->u.file.rdev      = -1;
  f->fs   = NULL;

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

struct Channel *
channel_ref(struct Channel *channel)
{
  k_spinlock_acquire(&channel_lock);
  channel->ref_count++;
  k_spinlock_release(&channel_lock);

  return channel;
}

// FIXME: this is a random guess
#define STATUS_MASK (O_APPEND | O_NONBLOCK | O_SYNC | O_RDONLY | O_RDWR | O_WRONLY)

int
channel_get_flags(struct Channel *channel)
{
  int r;

  k_spinlock_acquire(&channel_lock);
  r = channel->flags & STATUS_MASK;
  k_spinlock_release(&channel_lock);

  return r;
}

int
channel_set_flags(struct Channel *channel, int flags)
{
  k_spinlock_acquire(&channel_lock);
  channel->flags = (channel->flags & ~STATUS_MASK) | (flags & STATUS_MASK);
  k_spinlock_release(&channel_lock);
  return 0;
}

void
channel_send_recv(struct Channel *channel, struct IpcMessage *msg)
{
  //k_assert(channel->ref_count > 0);

  switch (channel->type) {
    case CHANNEL_TYPE_FILE:
      fs_send_recv(channel, msg);
      break;
    case CHANNEL_TYPE_PIPE:
      pipe_send_recv(channel, msg);
      break;
    case CHANNEL_TYPE_SOCKET:
      net_send_recv(channel, msg);
      break;
    default:
      k_panic("bad channel type %d", channel->type);
  }
}

void
channel_unref(struct Channel *channel)
{
  int ref_count;
  
  k_spinlock_acquire(&channel_lock);

  if (channel->ref_count < 1)
    k_panic("bad ref_count %d", ref_count);

  ref_count = --channel->ref_count;

  k_spinlock_release(&channel_lock);

  if (ref_count > 0)
    return;

  struct IpcMessage msg;

  msg.type = IPC_MSG_CLOSE;

  channel_send_recv(channel, &msg);

  if (channel->node != NULL) {
    fs_path_node_unref(channel->node);
    channel->node = NULL;
  }

  k_object_pool_put(channel_pool, channel);
}

off_t
channel_seek(struct Channel *channel, off_t offset, int whence)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_SEEK;
  msg.u.seek.offset = offset;
  msg.u.seek.whence = whence;

  channel_send_recv(channel, &msg);

  return msg.r;
}

ssize_t
channel_read(struct Channel *channel, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_READ;
  msg.u.read.va    = va;
  msg.u.read.nbyte = nbytes;

  channel_send_recv(channel, &msg);

  return msg.r;
}

ssize_t
channel_write(struct Channel *channel, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_WRITE;
  msg.u.write.va    = va;
  msg.u.write.nbyte = nbytes;

  channel_send_recv(channel, &msg);

  return msg.r;
}

ssize_t
channel_getdents(struct Channel *channel, uintptr_t va, size_t nbytes)
{
  struct IpcMessage msg;

  if ((channel->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;

  msg.type = IPC_MSG_READDIR;
  msg.u.readdir.va    = va;
  msg.u.readdir.nbyte = nbytes;
  
  channel_send_recv(channel, &msg);

  return msg.r;
}

int
channel_stat(struct Channel *channel, struct stat *buf)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSTAT;
  msg.u.fstat.buf  = buf;

  channel_send_recv(channel, &msg);

  return msg.r;
}

int
channel_chdir(struct Channel *channel)
{
  if (channel->node == NULL)
    return -ENOTDIR;

  return fs_path_set_cwd(channel->node);
}

int
channel_chmod(struct Channel *channel, mode_t mode)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FCHMOD;
  msg.u.fchmod.mode = mode;

  channel_send_recv(channel, &msg);

  return msg.r;
}

int
channel_chown(struct Channel *channel, uid_t uid, gid_t gid)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FCHOWN;
  msg.u.fchown.uid  = uid;
  msg.u.fchown.gid  = gid;

  channel_send_recv(channel, &msg);

  return msg.r;
}

int
channel_ioctl(struct Channel *channel, int request, int arg)
{
  struct IpcMessage msg;
  
  msg.type = IPC_MSG_IOCTL;
  msg.u.ioctl.request = request;
  msg.u.ioctl.arg     = arg;

  channel_send_recv(channel, &msg);

  return msg.r;
}

// TODO
int
channel_select(struct Channel *channel, struct timeval *timeout)
{
  switch (channel->type) {
  case CHANNEL_TYPE_FILE:
    return fs_select(channel, timeout);
  case CHANNEL_TYPE_SOCKET:
    return net_select(channel, timeout);
  case CHANNEL_TYPE_PIPE:
    return pipe_select(channel, timeout);
  default:
    k_panic("bad channel type %d", channel->type);
    return -EBADF;
  }
}

int
channel_truncate(struct Channel *channel, off_t length)
{
  struct IpcMessage msg;

  if ((channel->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  k_assert(channel->ref_count > 0);
  k_assert(channel->type == CHANNEL_TYPE_FILE);

  msg.type = IPC_MSG_TRUNC;
  msg.u.trunc.length = length;

  channel_send_recv(channel, &msg);

  return msg.r;
}

int
channel_sync(struct Channel *channel)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSYNC;

  channel_send_recv(channel, &msg);

  return msg.r;
}
