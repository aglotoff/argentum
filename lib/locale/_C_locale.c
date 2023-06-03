#include <limits.h>
#include <locale.h>

struct lconv _C_locale = {
  // LC_NUMERIC
  .decimal_point    = ".",
  .thousands_sep    = "",
  .grouping         = "",
  // LC_MONETARY
  .int_curr_symbol  = "",
  .currency_symbol  = "",
  .mon_decimal_point = "",
  .mon_thousands_sep = "",
  .mon_grouping      = "",
  .positive_sign     = "",
  .negative_sign     = "",
  .int_frac_digits   = CHAR_MAX,
  .frac_digits       = CHAR_MAX,
  .p_cs_precedes     = CHAR_MAX,
  .p_sep_by_space    = CHAR_MAX,
  .n_cs_precedes     = CHAR_MAX,
  .n_sep_by_space    = CHAR_MAX,
  .p_sign_posn       = CHAR_MAX,
  .n_sign_posn       = CHAR_MAX,
};
