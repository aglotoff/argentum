#include <sys/ipc.h>
#include <unistd.h>

_READ_WRITE_RETURN_TYPE
_read(int fildes, void *buf, size_t n)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_READ;
  msg.u.read.nbyte = n;

  return ipc_send(fildes, &msg, sizeof(msg), buf, n);
}

_READ_WRITE_RETURN_TYPE
read(int fildes, void *buf, size_t n)
{
  return _read(fildes, buf, n);
}
