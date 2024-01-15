#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

int
bug(void)
{
  int *p = (int *) 80000000;
  *p = 678;
  return 9;
}

int bog(void)
{
  return bug();
}

int big(void)
{
  int j = 0;
  for (int i = 10; i < 20; i++) {
    j += bog();
  }
  return bog();
}

int
main(void)
{
 
  uint64_t parsed_size;

  int scan = sscanf ("930      fdfd", "%" SCNu64, &parsed_size);

  printf("%d\n", scan);

  return 0;
}
