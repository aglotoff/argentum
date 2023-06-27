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

  printf("%d\n", fnmatch("[abc]", "a", 0));
  printf("%d\n", fnmatch("[!a-fy]der", "yder", 0));
  printf("%d\n", fnmatch("[c-]e", "-e", 0));
  printf("%d\n", fnmatch("[]fdff]e", "]e", 0));

  return 0;
}
