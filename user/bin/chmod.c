#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int
main(int argc, char **argv)
{
  mode_t mode;
  
  if (argc < 3) {
    printf("%s: missing operand\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  mode = (mode_t) strtol(argv[1], NULL, 8);

  if ((chmod(argv[2], mode)) < 0) {
    perror("chmod");
    exit(EXIT_FAILURE);
  }

  return 0;
}
