#include <sys/ipc.h>
#include <unistd.h>

int
ftruncate(int fildes, off_t length)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_TRUNC;
  msg.u.trunc.length = length;

  return ipc_send(fildes, &msg, sizeof msg, NULL, 0);
}
