#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("%s: missing operands\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (link(argv[1], argv[2]) != 0) {
    perror(argv[0]);
    exit(EXIT_FAILURE);
  }

  return 0;
}
