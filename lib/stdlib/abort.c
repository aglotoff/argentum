#include <stdlib.h>

/**
 * Generate an abnormal process abort.
 *
 * Cause abnormal process termination to occur, unless the signal SIGABRT is
 * being caught and the signal handler does not return.
 * 
 * @sa exit
 */
void
abort(void)
{
  // TODO: raise(SIGABRT)
  exit(EXIT_FAILURE);
}
