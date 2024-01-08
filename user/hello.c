#include <fnmatch.h>
#include <stdio.h>
#include <sys/syscall.h>

int
test(int a, int b, int c, int d, int e, int f)
{
  return __syscall(__SYS_TEST, a, b, c, d, e, f);
}

int
main(void)
{
  test(20, 50, 60, 700, 980, 1100);
  return 0;
}

