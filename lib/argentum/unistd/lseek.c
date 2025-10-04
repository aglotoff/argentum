#include <sys/ipc.h>
#include <unistd.h>

off_t
_lseek(int fildes, off_t offset, int whence)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_SEEK;
  msg.u.seek.offset = offset;
  msg.u.seek.whence = whence;

  return ipc_send(fildes, &msg, sizeof msg, NULL, 0);
}

off_t
lseek(int fildes, off_t offset, int whence)
{
  return _lseek(fildes, offset, whence);
}
