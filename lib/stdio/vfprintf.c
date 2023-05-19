#include <stdio.h>
#include <unistd.h>

static int
putch(void *arg, int ch)
{
  FILE *stream = (FILE *) arg;

  putc(ch, stream);

  return 1;
}

int
vfprintf(FILE *stream, const char *format, va_list ap)
{
  return __printf(putch, stream, format, ap);
}
