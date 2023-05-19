#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

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
  int i;

  while (__at_count > 0)
    __at_funcs[--__at_count]();

  // TODO: flush unwritten buffered data and close all open streams
  for (i = 0; i < FOPEN_MAX; i++) {
    if ((__files[i] != NULL) && (__files[i]->mode != 0))
      fclose(__files[i]);
  }

  _Exit(status);
}
