#include <stdio.h>
#include <unistd.h>
#include <time.h>

int
main(void)
{
  do {
    time_t t = time(NULL);
    printf("%s", asctime(gmtime(&t)));

    sleep(1);
  } while (1);

  return 0;
}
