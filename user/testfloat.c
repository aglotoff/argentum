// Test <float.h> macros
#include <float.h>
#include <stdio.h>

int
main(void)
{
  printf("FLT_RADIX = %d\n\n", FLT_RADIX);
  printf("FLT_DIG =        %5d   FLT_MANT_DIG =  %6d\n",
          FLT_DIG, FLT_MANT_DIG);
  printf("FLT_MAX_10_EXP = %5d   FLT_MAX_EXP  =  %6d\n",
          FLT_MAX_10_EXP, FLT_MAX_EXP);
  printf("FLT_MIN_10_EXP = %5d   FLT_MIN_EXP  =  %6d\n",
          FLT_MIN_10_EXP, FLT_MIN_EXP);
  printf("      FLT_EPSILON =  %f\n", FLT_EPSILON);
  printf("      FLT_MAX     =  %f\n", FLT_MAX);
  printf("      FLT_MIN     =  %f\n\n", FLT_MIN);
  
  printf("DBL_DIG =        %5d   DBL_MANT_DIG =  %6d\n",
          DBL_DIG, DBL_MANT_DIG);
  printf("DBL_MAX_10_EXP = %5d   DBL_MAX_EXP  =  %6d\n",
          DBL_MAX_10_EXP, DBL_MAX_EXP);
  printf("DBL_MIN_10_EXP = %5d   DBL_MIN_EXP  =  %6d\n",
          DBL_MIN_10_EXP, DBL_MIN_EXP);
  printf("      DBL_EPSILON =  %f\n", DBL_EPSILON);
  printf("      DBL_MAX     =  %f\n", DBL_MAX);
  printf("      DBL_MIN     =  %f\n\n", DBL_MIN);

  printf("LDBL_DIG =        %5d  LDBL_MANT_DIG =  %6d\n",
          LDBL_DIG, LDBL_MANT_DIG);
  printf("LDBL_MAX_10_EXP = %5d  LDBL_MAX_EXP  =  %6d\n",
          LDBL_MAX_10_EXP, LDBL_MAX_EXP);
  printf("LDBL_MIN_10_EXP = %5d  LDBL_MIN_EXP  =  %6d\n",
          LDBL_MIN_10_EXP, LDBL_MIN_EXP);
  printf("      LDBL_EPSILON = %f\n", LDBL_EPSILON);
  printf("      LDBL_MAX     = %f\n", LDBL_MAX);
  printf("      LDBL_MIN     = %f\n", LDBL_MIN);

  return 0;
}
