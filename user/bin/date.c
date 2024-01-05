#include <stdio.h>
#include <time.h>

int
main(void)
{
  time_t t;

  t = time(NULL);
  printf("%s", asctime(gmtime(&t)));

  return 0;
}
