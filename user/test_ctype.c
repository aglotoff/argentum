// Test <ctype.h> functions and macros
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

// Display a printable character class.
static void
print_class(const char *name, int (*f)(int))
{
  int c;

  printf("%s: ", name);
  for (c = EOF; c <= 0xFF; c++)
    if (f(c))
      printf("%c", c);
  printf("\n");
}

int
main(void)
{
  const char *s;
  int c;

  // Display printable classes
  print_class("ispunct", &ispunct);
  print_class("isdigit", &isdigit);
  print_class("islower", &islower);
  print_class("isupper", &isupper);
  print_class("isalpha", &isalpha);
  print_class("isalnum", &isalnum);

  // Test functions for required characters
  for (s = "0123456789"; *s; s++)
    assert((isdigit)(*s) && (isxdigit)(*s));
  for (s = "abcdefABCDEF"; *s; s++)
    assert((isxdigit)(*s));
  for (s = "abcdefghijklmnopqrstuvwxyz"; *s; s++)
    assert((islower)(*s));
  for (s = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; *s; s++)
    assert((isupper)(*s));
  for (s = "!\"#%&'();<=>?[\\]*+,-./:^_{|}~"; *s; s++)
    assert((ispunct)(*s));
  for (s = "\f\n\r\t\v"; *s; s++)
    assert((isspace)(*s) && (iscntrl)(*s));
  assert((isspace)(' ') && (isprint)(' '));
  assert((iscntrl)('\a') && (iscntrl)('\b'));

  // Test functions for proper class membership of all valid codes
  for (c = EOF; c <= 0xFF; c++) {
    if ((isdigit)(c))
      assert((isalnum)(c) && (isxdigit)(c));
    if ((isupper)(c))
      assert((isalpha)(c));
    if ((islower)(c))
      assert((isalpha)(c));
    if ((isalpha)(c))
      assert((isalnum)(c) && !(isdigit)(c));
    if ((isalnum)(c))
      assert((isgraph)(c) && !(ispunct)(c));
    if ((ispunct)(c))
      assert((isgraph)(c) && !(isalnum)(c));
    if ((isgraph)(c))
      assert((isprint)(c) && (c != ' '));
    if ((isspace)(c))
      assert((c == ' ') || !(isprint)(c));
    if ((iscntrl)(c))
      assert(!(isalnum)(c));
  }

  // Test macros for required characters
  for (s = "0123456789"; *s; s++)
    assert(isdigit(*s) && isxdigit(*s));
  for (s = "abcdefABCDEF"; *s; s++)
    assert(isxdigit(*s));
  for (s = "abcdefghijklmnopqrstuvwxyz"; *s; s++)
    assert(islower(*s));
  for (s = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; *s; s++)
    assert(isupper(*s));
  for (s = "!\"#%&'();<=>?[\\]*+,-./:^_{|}~"; *s; s++)
    assert(ispunct(*s));
  for (s = "\f\n\r\t\v"; *s; s++)
    assert(isspace(*s) && iscntrl(*s));
  assert(isspace(' ') && isprint(' '));
  assert(iscntrl('\a') && iscntrl('\b'));

  // Test macros for proper class membership of all valid codes
  for (c = EOF; c <= 0xFF; c++) {
    if (isdigit(c))
      assert(isalnum(c) && isxdigit(c));
    if (isupper(c))
      assert(isalpha(c));
    if (islower(c))
      assert(isalpha(c));
    if (isalpha(c))
      assert(isalnum(c) && !isdigit(c));
    if (isalnum(c))
      assert(isgraph(c) && !ispunct(c));
    if (ispunct(c))
      assert(isgraph(c) && !isalnum(c));
    if (isgraph(c))
      assert(isprint(c) && (c != ' '));
    if (isspace(c))
      assert((c == ' ') || !isprint(c));
    if (iscntrl(c))
      assert(!isalnum(c));
  }

  printf("SUCCESS testing <ctype.h>\n");
  return 0;
}
