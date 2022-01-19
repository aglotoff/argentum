#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Function to compare elements in an array of characters.
static int
cmp(const void *p1, const void *p2)
{
  char c1 = *(const char *) p1;
  char c2 = *(const char *) p2;

  return c1 - c2;
}

int
main(void)
{
  static char abc[] = "abcdefghijklmnopqrstufvxyz";

  int i1, i2;
  char *s1, *s2;

  (void) s2;

  assert(32767 <= RAND_MAX);

  // --------------------------------------------------------------------------
  // Numeric conversion functions
  // --------------------------------------------------------------------------

  assert((atoi("37") == 37) && (atoi("-7192x") == -7192));
  assert((atol("+29") == 29L) && (atoi("-077") == -77L));
  assert((strtol("-a0", &s1, 11) == -110) && (s1 != NULL) && (*s1 == '\0'));
  assert((strtoul("54", &s1, 4) == 0) && (s1 != NULL) && (*s1 == '5'));
  assert((strtoul("0xFfg", &s1, 16) == 255) && (s1 != NULL) && (*s1 == 'g'));

  // --------------------------------------------------------------------------
  // Pseudo-random sequence generation functions
  // --------------------------------------------------------------------------

  assert(((i1 = rand()) >= 0) && (i1 <= RAND_MAX));
  assert(((i2 = rand()) >= 0) && (i2 <= RAND_MAX));
  srand(1);
  assert((rand() == i1) && (rand() == i2));

  // --------------------------------------------------------------------------
  // Searching and sorting utilities
  // --------------------------------------------------------------------------

  assert(bsearch("0", abc, sizeof(abc) - 1, 1, cmp) == NULL);
  assert(bsearch("d", abc, sizeof(abc) - 1, 1, cmp) == &abc[3]);

  // --------------------------------------------------------------------------
  // Integer arithmetic functions
  // --------------------------------------------------------------------------

  assert((abs(-4) == 4) && (abs(4) == 4));

  assert((div(7, 2).quot == 3) && (div(7, 2).rem == 1));
  assert((div(-7, 2).quot == -3) && (div(-7, 2).rem == -1));

  assert((labs(-4L) == 4L) && (labs(4L) == 4L));

  assert((ldiv(7L, 2L).quot == 3L) && (ldiv(7L, 2L).rem == 1L));
  assert((ldiv(-7L, 2L).quot == -3L) && (ldiv(-7L, 2L).rem == -1L));

  printf("SUCCESS testing <stdlib.h>\n");
  
  return 0;
}
