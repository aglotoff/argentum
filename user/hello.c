#include <stdio.h>

int
main(int argc, const char **argv)
{
  int i;

  printf("Hello world! %d\n", argc);

  for (i = 0; i < argc; i++)
    printf("%d: %s\n", i, argv[i]);

  return 0;
}
