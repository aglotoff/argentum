// Test <string.h> functions
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int
main(void)
{
  static const char abcde[] = "abcde";
  static const char abcdx[] = "abcdx";

  char s[20];

  // memchr
  assert(memchr(abcde, 'c', 5) == &abcde[2]);
  assert(memchr(abcde, 'e', 4) == NULL);

  // memcmp
  assert(memcmp(abcde, abcdx, 5) < 0);
  assert(memcmp(abcdx, abcde, 5) > 0);
  assert(memcmp(abcde, abcdx, 4) == 0);

  // memcpy
  assert((memcpy(s, abcde, 6) == s) && (s[2] == 'c'));

  // memmove
  assert(memmove(s, s + 1, 3) == s);
  assert(memcmp(memmove(s, s + 1, 3), "aabce", 6) > 0);
  assert(memcmp((char *) memmove(s + 2, s, 3) - 2, "bcece", 6) > 0);

  // memset
  assert((memset(s, '*', 10) == s) && (s[9] == '*'));
  assert((memset(s + 2, '%', 0) == s + 2) && (s[2] == '*'));

  // strcat
  assert(strcat(memcpy(s, abcde, 6), "fg") == s);
  assert(s[6] == 'g');

  // strchr
  assert(strchr(abcde, 'x') == NULL);
  assert(strchr(abcde, 'c') == &abcde[2]);
  assert(strchr(abcde, '\0') == &abcde[5]);

  // strcmp
  assert(strcmp(abcde, abcdx) < 0);
  assert(strcmp(abcdx, abcde) > 0);
  assert(strcmp(abcde, "abcde") == 0);

  // TODO: strcoll

  // strcpy
  assert((strcpy(s, abcde) == s) && (strcmp(s, abcde) == 0));

  // strcspn
  assert(strcspn(abcde, "xdy") == 3);
  assert(strcspn(abcde, abcde) == 0);
  assert(strcspn(abcde, "xyz") == 5);

  // strerror
  assert(strerror(EDOM) != NULL);

  // strlen
  assert(strlen(abcde) == 5);
  assert(strlen("") == 0);

  // strncat
  assert(strncat(strcpy(s, abcde), "fg", 1) == s);
  assert(strcmp(s, "abcdef") == 0);

  // strncmp
  assert(strncmp(abcde, "abcde", 30) == 0);
  assert(strncmp(abcde, abcdx, 30) < 0);
  assert(strncmp(abcdx, abcde, 30) > 0);
  assert(strncmp(abcde, abcdx, 4) == 0);

  // strncpy
  s[6] = '*';
  assert((strncpy(s, abcde, 7) == s) && (memcmp(s, "abcde\0\0", 7) == 0));
  assert((strncpy(s, "xyz", 2) == s) && (memcmp(s, "xycde\0\0", 7) == 0));
  
  // strpbrk
  assert(strpbrk(abcde, "xdy") == &abcde[3]);
  assert(strpbrk(abcde, "xyz") == NULL);

  // strrchr
  assert(strrchr(abcde, 'x') == NULL);
  assert(strrchr(abcde, 'c') == &abcde[2]);
  assert(strrchr(abcde, '\0') == &abcde[5]);
  assert(strcmp(strrchr("ababa", 'b'), "ba") == 0);

  // strspn
  assert(strspn(abcde, "abce") == 3);
  assert(strspn(abcde, abcde) == 5);
  assert(strspn(abcde, "xyz") == 0);

  // strtok
  assert(strtok(strcpy(s, abcde), "ac") == &s[1]);
  assert(strtok(NULL, "ace") == &s[3]);
  assert((strtok(NULL, "ace") == NULL) && (memcmp("ab\0d\0\0", s, 6) == 0));

  // TODO: strxfrm

  printf("SUCCESS testing <string.h>\n");
  return 0;
}
