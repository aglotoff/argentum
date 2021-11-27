#include <stdio.h>

struct snprintfbuf {
  char   *s;
  size_t  idx;
  size_t  n;
};

static int
putch(void *arg, int ch)
{
  struct snprintfbuf *buf = (struct snprintfbuf *) arg;

  if (buf->idx < (buf->n - 1)) {
    buf->s[buf->idx++] = ch;
    return 1;
  }
  return 0;
}

int
vsnprintf(char *s, size_t n, const char *format, va_list ap)
{
  struct snprintfbuf buf;

  buf.s   = s;
  buf.idx = 0;
  buf.n   = n;

  __printf(putch, &buf, format, ap);

  buf.s[buf.idx] = '\0';

  return buf.idx;
}
