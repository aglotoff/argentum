#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int
main(int argc, char **argv)
{
  if (argc < 2) {
    printf("%s: missing operand\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (mkdir(argv[1], 0755) < 0) {
    perror(argv[1]);
    exit(EXIT_FAILURE);
  }

  return 0;
}
