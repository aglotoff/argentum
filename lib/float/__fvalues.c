#include <float.h>

#if __D0 != 0
  #define FLT_INIT(u0, ux)	    { { (u0), (u0) } }
  #define DBL_INIT(u0, ux)	    { { (ux), (ux), (ux), (u0) } }
  #define LDBL_INIT(u0, u1, ux) { { (ux), (ux), (ux), (u1), (u0) } }
#else
  #define FLT_INIT(u0, ux)	    { { (u0), (ux) } }
  #define DBL_INIT(u0, ux)	    { { (u0), (ux), (ux), (ux) } }
  #define LDBL_INIT(u0, u1, ux)	{ { (u0), (u1), (ux), (ux), (ux) } }
#endif

struct __flt __flt = {
  .max = FLT_INIT((__FLT_EMAX << __FLT_EOFF) - 1, ~0),
  .min = FLT_INIT((1 << __FLT_EOFF), 0),
  .eps = FLT_INIT((__FLT_EBIAS - __FLT_FBITS + 1) << __FLT_EOFF, 0),
};

struct __dbl __dbl = {
  .max = DBL_INIT((__DBL_EMAX << __DBL_EOFF) - 1, ~0),
  .min = DBL_INIT((1 << __DBL_EOFF), 0),
  .eps = DBL_INIT((__DBL_EBIAS - __DBL_FBITS + 1) << __DBL_EOFF, 0),
};

#if __LDBL
struct __ldbl __ldbl = {
  .max = LDBL_INIT(__LDBL_EMAX - 1, ~0, ~0),
  .min = LDBL_INIT(1, (1 << __LDBL_EOFF), 0),
  .eps = LDBL_INIT(__LDBL_EBIAS - __LDBL_FBITS + 1, (1 << __LDBL_EOFF), 0),
};
#endif  // __LDBL
