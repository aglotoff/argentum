#include <argentum/syscall.h>
#include <string.h>

int x = 78;

int
main(void)
{
  const char *s = "Hello World!\n";

  cwrite(s, strlen(s));
  return 0;
}
