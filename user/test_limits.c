// Test <limits.h> macros
#include <limits.h>
#include <stdio.h>

int
main(void)
{
  printf("CHAR_BIT   = %2d    MB_LEN_MAX = %2d\n\n",
          CHAR_BIT, MB_LEN_MAX);

  printf("  CHAR_MAX = %+20d    CHAR_MIN = %20d\n",
         CHAR_MAX, CHAR_MIN);
  printf(" SCHAR_MAX = %+20d   SCHAR_MIN = %20d\n",
         SCHAR_MAX, SCHAR_MIN);
  printf(" UCHAR_MAX = %+20u\n\n", UCHAR_MAX);
  
  printf("  SHRT_MAX = %+20d    SHRT_MIN = %20d\n",
         SHRT_MAX, SHRT_MIN);
  printf(" USHRT_MAX = %+20u\n\n", USHRT_MAX);

  printf("   INT_MAX = %+20d     INT_MIN = %20d\n",
         INT_MAX, INT_MIN);
  printf("  UINT_MAX = %+20u\n\n", UINT_MAX);

  printf("  LONG_MAX = %+20ld    LONG_MIN = %20ld\n",
         LONG_MAX, LONG_MIN);
  printf(" ULONG_MAX = %+20lu\n\n", ULONG_MAX);

  printf(" LLONG_MAX = %+20lld   LLONG_MIN = %20lld\n",
          LLONG_MAX, LLONG_MIN);
  printf("ULLONG_MAX = %20llu\n", ULLONG_MAX);

  printf("SUCCESS testing <limits.h>\n");

  return 0;
}
