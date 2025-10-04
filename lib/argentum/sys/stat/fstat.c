#include <sys/stat.h>
#include <sys/ipc.h>

int
_fstat(int fildes, struct stat *buf)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSTAT;

  return ipc_send(fildes, &msg, sizeof msg, buf, sizeof(struct stat));
}

int
fstat(int fildes, struct stat *buf)
{
  return _fstat(fildes, buf);
}
