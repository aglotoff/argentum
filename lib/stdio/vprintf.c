#include <stdio.h>
#include <user.h>

#define MAXBUF  256

struct printfbuf {
  char data[MAXBUF];
  int  idx;
};

static int
putch(void *arg, int ch)
{
  struct printfbuf *buf = (struct printfbuf *) arg;

  buf->data[buf->idx++] = ch;
  if (buf->idx == (MAXBUF - 1)) {
    cwrite(buf->data, buf->idx);
    buf->idx = 0;
  }
  return 1;
}

int
vprintf(const char *format, va_list ap)
{
  struct printfbuf buf;
  int ret;

  buf.idx = 0;

  ret = __printf(putch, &buf, format, ap);

  if (cwrite(buf.data, buf.idx) < 0)
    return -1;

  return ret + buf.idx;
}
