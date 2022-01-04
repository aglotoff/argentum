// Test <errno.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>

int
main(void)
{
  assert(errno == 0);
  perror("No error reported as");

  errno = ERANGE;
  assert(errno == ERANGE);
  perror("ERANGE reported as");

  printf("SUCCESS testing <errno.h>\n");

  return 0;
}
