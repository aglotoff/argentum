#include <stdio.h>
#include <time.h>

#define BUF_SIZE  64 

char *
asctime(const struct tm *timeptr)
{
  static char buf[BUF_SIZE];

  strftime(buf, BUF_SIZE, "%a %b %d %H:%M:%S %Y", timeptr);
  return buf;
}
