#include <locale.h>

struct lconv *
localeconv(void)
{
  return &_C_locale;
}
