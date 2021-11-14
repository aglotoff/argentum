#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Write error message to standard error.
 * 
 * @param s C string containing a custom message to be printed before the
 *          error message, or null pointer.
 */
void
perror(const char *s)
{
  if (s)
    printf("%s: ", s);
  printf("%s\n", strerror(errno));
}
