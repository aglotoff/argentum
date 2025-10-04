#include <sys/ipc.h>
#include <unistd.h>

int
fchown(int fildes, uid_t owner, gid_t group)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_FCHOWN;
  msg.u.fchown.uid  = owner;
  msg.u.fchown.gid  = group;

  return ipc_send(fildes, &msg, sizeof msg, NULL, 0);
}
