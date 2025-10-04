#include <sys/ipc.h>
#include <unistd.h>

_READ_WRITE_RETURN_TYPE
_write(int fildes, const void *buf, size_t n)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_WRITE;
  msg.u.write.nbyte = n;

  struct iovec send_iov[] = {
    { &msg, sizeof(msg) },
    { (void *) buf, n },
  };

  return ipc_sendv(fildes, send_iov, 2, NULL, 0);
}

_READ_WRITE_RETURN_TYPE
write(int fildes, const void *buf, size_t n)
{
  return _write(fildes, buf, n);
}
