#include <fnmatch.h>
#include <stdio.h>

int
main(void)
{
  const char *text = "baaabab";

  printf("%d\n", fnmatch("*****ba*****ab", text, 0));
  printf("%d\n", fnmatch("baaa?ab", text, 0));
  printf("%d\n", fnmatch("ba*a?", text, 0));
  printf("%d\n", fnmatch("a*ab", text, 0));

  return 0;
}
