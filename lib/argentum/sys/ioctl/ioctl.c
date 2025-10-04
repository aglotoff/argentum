#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <termios.h>

int
ioctl(int fildes, int request, ...)
{
  struct IpcMessage msg;
  int arg = 0;

  if (IOCPARM_LEN(request) != 0) {
    va_list ap;

    va_start(ap, request);
    arg = va_arg(ap, int);
    va_end(ap);
  }

  msg.type = IPC_MSG_IOCTL;
  msg.u.ioctl.request = request;
  msg.u.ioctl.arg     = arg;

  struct iovec send_iov[] = {
    { &msg, sizeof(msg) },
    { (void *) arg, 0 },
  };

  switch (request) {
  case TIOCGETA:
    return ipc_send(fildes, &msg, sizeof(msg), (void *) arg, sizeof(struct termios));
  case TIOCGWINSZ:
    return ipc_send(fildes, &msg, sizeof(msg), (void *) arg, sizeof(struct winsize));
  case TIOCSETAW:
  case TIOCSETA:
    send_iov[1].iov_len = sizeof(struct termios);
    return ipc_sendv(fildes, send_iov, 2, NULL, 0);
  case TIOCSWINSZ:
    send_iov[1].iov_len = sizeof(struct winsize);
    return ipc_sendv(fildes, send_iov, 2, NULL, 0);
  default:
    return ipc_send(fildes, &msg, sizeof(msg), NULL, 0);
  }
}
