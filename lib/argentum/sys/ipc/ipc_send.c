#include <sys/ipc.h>
#include <sys/syscall.h>

intptr_t
ipc_send(int fildes, void *smsg, size_t sbytes, void *rmsg, size_t rbytes)
{
  return __syscall5(__SYS_IPC_SEND, fildes, smsg, sbytes, rmsg, rbytes);
}
