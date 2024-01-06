#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// TODO: nanosleep
unsigned
sleep(unsigned seconds)
{
  fprintf(stderr, "TODO: sleep(%u)\n", seconds);
  abort();

  return -1;
}
