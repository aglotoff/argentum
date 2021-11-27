#include <stdio.h>
#include <unistd.h>

int
main(void)
{
  printf("Characters: %c %c \n", 'a', 65);
  printf("Decimals: %d %ld\n", 1977, 650000L);
  printf("Preceding with blanks: %10d \n", 1977);
  printf("Preceding with zeros: %010d \n", 1977);
  printf("Some different radices: %d %x %o %#x %#o \n", 100, 100, 100, 100, 100);
  printf("Floating point\n");
  printf("Rounding: %f %.0f %.32f\n", 1.5, 1.5, 1.3);
  printf("Padding: %05.2f %.2f %5.2f\n", 1.5, 1.5, 1.5);
  printf("Width trick: %*d \n", 5, 10);
  printf("%s \n", "A string");

  return 0;
}
