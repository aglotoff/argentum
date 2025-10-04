#include <sys/ipc.h>
#include <sys/syscall.h>

intptr_t
ipc_sendv(int fildes, struct iovec *send_iov, int send_iov_cnt, struct iovec *recv_iov, int recv_iov_cnt)
{
  return __syscall5(__SYS_IPC_SENDV, fildes, send_iov, send_iov_cnt, recv_iov, recv_iov_cnt);
}
