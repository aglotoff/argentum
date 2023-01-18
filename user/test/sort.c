#include <stdlib.h>
#include <stdio.h>

static int
cmp(const void *a, const void *b)
{
  return *(int *) a - *(int *) b;
}

int
main(void)
{
  int i;
  
  int a[] = { 4, -6, 32, 1, -3, 43, 56, 78, 89, -9, 32, 11, 59, -456, 45,
              32, 76, 11, -89, -4, 4, 78, 1, 45, -6, 78, -6, 2, 43, 59 };

  for (i = 0; i < 30; i++) {
    printf(" %d", a[i]);
  }
  printf("\n");

  qsort(a, 30, sizeof(int), cmp);

  for (i = 0; i < 30; i++) {
    printf(" %d", a[i]);
  }
  printf("\n");

  return 0;
}
