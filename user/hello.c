#include <stdio.h>
#include <unistd.h>

void f(int i)
{
  if (i >= 10000)
    return;
  printf("%d\n", i);
  f(i + 1);
}

int
main(void)
{
  // if (fork() == 0) {
  //   for (double x = 10000000000.0; x < 20000000000.0; x += 0.1)
  //     printf("1: %f\n", x);
  // } else {
  //   for (double x = 20000000000.0; x < 30000000000.0; x += 0.1)
  //     printf("2: %f\n", x);
  // }
  f(0);
  return 0;
}
