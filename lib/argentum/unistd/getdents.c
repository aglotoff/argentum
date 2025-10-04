#include <sys/ipc.h>
#include <sys/types.h>

ssize_t
getdents(int fd, void *buf, size_t n)
{
  struct IpcMessage msg;

  msg.type = IPC_MSG_READDIR;
  msg.u.readdir.nbyte = n;

  return ipc_send(fd, &msg, sizeof msg, buf, n);
}
