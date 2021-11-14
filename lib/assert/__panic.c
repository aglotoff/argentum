#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void
__panic(const char *file, int line, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  printf("Error at %s:%d: ", file, line);
  vprintf(format, ap);
  printf("\n");

  va_end(ap);
  
  abort();
}
