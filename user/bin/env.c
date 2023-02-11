#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
  char **e;

  for (e = environ; *e != NULL; e++)
    printf("%s\n", *e);

  return 0;
}
