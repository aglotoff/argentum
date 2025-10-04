#include <sys/ipc.h>
#include <unistd.h>

int 
_fsync(int fildes)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FSYNC;

  return ipc_send(fildes, &msg, sizeof msg, NULL, 0);
}

int
fsync(int fildes)
{
  return _fsync(fildes);
}
