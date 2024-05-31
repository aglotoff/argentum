#include <arpa/inet.h>
#include <stdio.h>

int
main(void) {
  struct in_addr aton = { 0xff };

  inet_aton("127.2.1r6.0", &aton);
  printf("%s\n", inet_ntoa(aton));
  return 0;
}
