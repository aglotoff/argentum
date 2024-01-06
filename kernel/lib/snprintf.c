#include <stdarg.h>
#include <stdio.h>

int   __printf(int (*)(void *, int), void *, const char *, va_list);

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

int
snprintf(char *s, size_t n, const char *format, ...)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vsnprintf(s, n, format, ap);
  va_end(ap);

  return ret;
}
