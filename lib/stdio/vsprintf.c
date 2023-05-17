#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>

struct sprintfbuf {
  char   *s;
  size_t  idx;
};

static int
putch(void *arg, int ch)
{
  struct sprintfbuf *buf = (struct sprintfbuf *) arg;

  buf->s[buf->idx++] = ch;
  return 1;
}

int
vsprintf(char *s, const char *format, va_list ap)
{
  struct sprintfbuf buf;

  buf.s   = s;
  buf.idx = 0;

  __printf(putch, &buf, format, ap);

  buf.s[buf.idx] = '\0';

  return buf.idx;
}
