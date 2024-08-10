#include <stdio.h>
#include <setjmp.h>

int
main(void) {
  sigjmp_buf env;
  
  sigsetjmp(env, 0);

  return 0;
}
