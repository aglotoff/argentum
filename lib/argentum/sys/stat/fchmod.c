#include <sys/stat.h>
#include <sys/ipc.h>

int
fchmod(int fildes, mode_t mode)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FCHMOD;
  msg.u.fchmod.mode = mode;

  return ipc_send(fildes, &msg, sizeof(msg), NULL, 0);
}
