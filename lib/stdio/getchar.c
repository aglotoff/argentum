#include <stdio.h>
#include <stdlib.h>

int
(getchar)(void)
{
  return getc(stdin);
}
