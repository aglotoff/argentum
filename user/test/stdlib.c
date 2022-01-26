#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  // Memory-management functions
  // --------------------------------------------------------------------------

  assert((s1 = (char *) malloc(sizeof(abc))) != NULL);
  strcpy(s1, abc);
  assert(strcmp(s1, abc) == 0);

  assert(((s2 = (char *) calloc(sizeof(abc), 1)) != NULL) && (s2[0] == '\0'));
  assert(memcmp(s2, s2 + 1, sizeof(abc) - 1) == 0);
  free(s2);

  assert((s1 = realloc(s1, sizeof(abc) * 2 - 1)) != NULL);
  strcat(s1, abc);
  assert(strrchr(s1, 'z') == (s1 + strlen(abc) * 2 - 1));

  assert((s1 = realloc(s1, sizeof(abc) - 3)) != NULL);
  assert(memcmp(s1, abc, sizeof(abc) - 3) == 0);
  free(s1);

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
