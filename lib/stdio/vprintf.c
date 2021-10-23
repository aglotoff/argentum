#include <stdio.h>
#include <user.h>

#define MAXBUF  256

struct printfbuf {
  char data[MAXBUF];
  int  idx;
  int  cnt;
};

static void
putch(void *arg, int ch)
{
  struct printfbuf *buf = (struct printfbuf *) arg;

  buf->data[buf->idx++] = ch;
  if (buf->idx == (MAXBUF - 1)) {
    cwrite(buf->data, buf->idx);
    buf->idx = 0;
  }
  buf->cnt++;
}

int
vprintf(const char *format, va_list ap)
{
  struct printfbuf buf;

  buf.idx = 0;
  buf.cnt = 0;

  xprintf(putch, &buf, format, ap);

  if (cwrite(buf.data, buf.idx) < 0)
    return -1;

  return buf.cnt;
}
