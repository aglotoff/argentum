#include <stdio.h>
#include <unistd.h>

#define MAXBUF  256

struct printfbuf {
  int  fd;
  char data[MAXBUF];
  int  idx;
};

static int
putch(void *arg, int ch)
{
  struct printfbuf *buf = (struct printfbuf *) arg;

  buf->data[buf->idx++] = ch;
  if (buf->idx == (MAXBUF - 1)) {
    write(buf->fd, buf->data, buf->idx);
    buf->idx = 0;
  }
  return 1;
}

int
vfprintf(FILE *stream, const char *format, va_list ap)
{
  struct printfbuf buf;
  int ret;

  buf.fd  = stream->fd;
  buf.idx = 0;

  ret = __printf(putch, &buf, format, ap);

  if (write(stream->fd, buf.data, buf.idx) < 0)
    return -1;

  return ret + buf.idx;
}
