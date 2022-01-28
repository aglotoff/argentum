#include <limits.h>
#include <stdlib.h>

void (*__at_funcs[ATEXIT_MAX])(void);
size_t __at_count;

/**
 * Terminate a process.
 * 
 * @param status Exit status of the process.
 * 
 * @sa abort
 */
void
exit(int status)
{
  while (__at_count > 0)
    __at_funcs[--__at_count]();

  // TODO: flush unwritten buffered data and close all open streams

  _Exit(status);
}
