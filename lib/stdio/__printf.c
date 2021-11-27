#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct pft {
  int   (*putc)(void *, int);
  void   *putc_arg;
  int     nchar;
  int     flags;
  int     width;
  int     precision;
  int     length;
  char    conversion;
};

enum {
  FLAG_LEFT  = (1 << 0),  // left-justified
  FLAG_SIGN  = (1 << 1),  // always display the sign character
  FLAG_BLANK = (1 << 2),  // print space if there is no sign character
  FLAG_ALT   = (1 << 3),  // alternate form
  FLAG_ZERO  = (1 << 4),  // print leading zeros
};

static long long          get_int_arg(va_list *, int);
static unsigned long long get_uint_arg(va_list *, int);
static long double        get_double_arg(va_list *, int);
static void               print_char(struct pft *, char);
static void               print_int(struct pft *, long long);
static void               print_double(struct pft *, long double);
static void               print_str(struct pft *, const char *);

/**
 * Generic function to print formatted data.
 *
 * @param xputc Pointer to the output function.
 * @param xputc_arg The first argument for the output function.
 * @param format The format string.
 * @param ap A variable argument list.
 */
int 
__printf(int       (*xputc)(void *, int),
         void       *xputc_arg,
         const char *format,
         va_list     ap)
{
  struct pft pft = {
    .putc     = xputc,
    .putc_arg = xputc_arg,
    .nchar    = 0,
  };
  long long num;
  long double ld;
  char *str;

  while (*format != '\0') {
    pft.width = pft.precision = -1;
    pft.length = 0;

    // Print literally
    while ((*format != '\0') && (*format != '%'))
      print_char(&pft, *format++);

    if (*format == '\0')
      break;

    format++;

    // Flags
    pft.flags = 0;
    while (strchr("-+ #0", *format)) {
      switch (*format++) {
      case '-':
        pft.flags |= FLAG_LEFT; break;
      case '+':
        pft.flags |= FLAG_SIGN; break;
      case ' ':
        pft.flags |= FLAG_BLANK; break;
      case '#':
        pft.flags |= FLAG_ALT; break;
      case '0':
        pft.flags |= FLAG_ZERO; break;
      }
    }

    // Get field width.
    if (*format == '*') {
      format++;
      pft.width = va_arg(ap, unsigned);
      if (pft.width < 0) {
        // Same as '-' flag.
        pft.width = -pft.width;
        pft.flags |= FLAG_LEFT; break;
      }
    } else if ((*format >= '0') && (*format <= '9')) {
      pft.width = 0;
      while ((*format >= '0') && (*format <= '9')) {
        pft.width = pft.width * 10 + (*format - '0');
        format++;
      }
    }

    // Get field precision.
    if (*format == '.') {
      format++;
      pft.precision = 0;
      if (*format == '*') {
        format++;
        pft.precision = va_arg(ap, int);
      } else {
        while ((*format >= '0') && (*format <= '9')) {
          pft.precision = pft.precision * 10 + (*format - '0');
          format++;
        }
      }
    }

    // Length modifier
    pft.length = 0;
    while ((*format == 'l') || (*format == 'h')) {
      switch (*format) {
      case 'h':
        pft.length--;
        break;
      case 'l':
      case 'L':
        pft.length++;
        break;
      }
      format++;
    }

    // Do conversion
    pft.conversion = *format;
    switch (pft.conversion) {
      // Signed decimal
      case 'd':
      case 'i':
        num = get_int_arg(&ap, pft.length);
        print_int(&pft, num);
        break;

      // Unsigned decimal, octal, hexadecimal
      case 'u':
      case 'o':
      case 'O':
      case 'x':
      case 'X':
        num = get_uint_arg(&ap, pft.length);
        print_int(&pft, num);
        break;

      // Double
      case 'f':
      case 'F':
      case 'a':
      case 'A':
        ld = get_double_arg(&ap, pft.length);
        print_double(&pft, ld);
        break;

      // Pointer
      case 'p':
        pft.width = sizeof(uintptr_t) * 2 + 2;
        pft.flags |= FLAG_ZERO | FLAG_ALT;
        num = (uintptr_t) va_arg(ap, void *);
        print_int(&pft, num);
        break;

      // Character
      case 'c':
        num = va_arg(ap, int);
        print_char(&pft, (unsigned char) num);
        break;

      // C string
      case 's':
        if (!(str = va_arg(ap, char *)))
          str = "(null)";
        print_str(&pft, str);
        break;

      // Store the number of characters written so far
      case 'n':
        *(va_arg(ap, int *)) = pft.nchar;
        break;
      
      // A % character
      case '%':
        print_char(&pft, '%');
        break;

      // Unknown conversion specifier, print the whole sequence literally
      default:
        while (*format != '%')
          format--;
        break;
    }

    format++;
  }

  return pft.nchar;
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions to fetch var args
 * ----------------------------------------------------------------------------
 */

// Take the next integer argument of a given length from the variable argument
// list
static long long
get_int_arg(va_list *ap, int length)
{
  if (length > 1)
    return va_arg(*ap, long long);
  if (length == 1)
    return va_arg(*ap, long);
  if (length == 0)
    return va_arg(*ap, int);
  if (length == -1)
    return (short) va_arg(*ap, int);
  return (signed char) va_arg(*ap, int);
}

// Take the next unsigned integer argument of a given length from the variable
// argument list.
static unsigned long long
get_uint_arg(va_list *ap, int length)
{
  if (length > 1)
    return va_arg(*ap, unsigned long long);
  if (length == 1)
    return va_arg(*ap, unsigned long);
  if (length == 0)
    return va_arg(*ap, unsigned);
  if (length == -1)
    return (unsigned short) va_arg(*ap, unsigned);
  return (unsigned char) va_arg(*ap, unsigned);
}

// Take the next double argument of a given length from the variable argument
// list
static long double
get_double_arg(va_list *ap, int length)
{
  if (length > 0)
    return va_arg(*ap, long double);
  return va_arg(*ap, double);
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions to print the conversion results
 * ----------------------------------------------------------------------------
 */

// Print a single character keeping track of the number of characters printed
// so far
static void
print_char(struct pft *pft, char c)
{
  pft->nchar += pft->putc(pft->putc_arg, c);
}

// Print a formatted integer.
static void
print_int(struct pft *pft, long long num)
{
  char prefix[3], digits[64];
  unsigned long long abs_num;
  int base, sign, upper;
  int nprefix, ndigits, nzeros, nblanks;
  char *symbols;

  if ((pft->conversion == 'o') || (pft->conversion == 'O'))
    base = 8;
  else if ((pft->conversion == 'x') || (pft->conversion == 'X'))
    base = 16;
  else
    base = 10;

  sign    = ((pft->conversion == 'd') || (pft->conversion == 'i'));
  upper   = ((pft->conversion == 'O') || (pft->conversion == 'X'));
  symbols = upper ? "0123456789ABCDEF" : "0123456789abcdef";

  abs_num = num;
  if (sign && (num < 0))
    abs_num = -abs_num;

  nprefix = ndigits = nzeros = nblanks = 0;

  // Convert the alternate form prefix (store in reverse order).
  if (pft->flags & FLAG_ALT) {
    if (base == 8) {
      prefix[nprefix++] = '0';
    } else if (base == 16) {
      prefix[nprefix++] = upper ? 'X' : 'x';
      prefix[nprefix++] = '0';
    }
  }

  // Convert the sign character.
  if (sign && (num < 0))
    prefix[nprefix++] = '-';
  else if (pft->flags & FLAG_SIGN)
    prefix[nprefix++] = '+';
  else if (pft->flags & FLAG_BLANK)
    prefix[nprefix++] = ' ';

  // Convert the digits (store in reverse order).
  do {
    digits[ndigits++] = symbols[abs_num % base];
  } while ((abs_num /= base) != 0);

  // Determine the number of leading zeros and padding characters
  if (pft->precision > ndigits)
    nzeros = pft->precision - ndigits;
  if (pft->width > (nprefix + nzeros + ndigits)) {
    if ((pft->flags & FLAG_ZERO) && !(pft->flags & FLAG_LEFT)) {
      nzeros  = pft->width - (nprefix + ndigits);
      nblanks = 0;
    } else {
      nblanks = pft->width - (nprefix + nzeros + ndigits);
    }
  }

  if (!(pft->flags & FLAG_LEFT))
    while (nblanks-- > 0)
      print_char(pft, ' ');

  while (nprefix > 0)
    print_char(pft, prefix[--nprefix]);
    
  while (nzeros-- > 0)
      print_char(pft, '0');
  
  while (ndigits > 0)
    print_char(pft, digits[--ndigits]);

  while (nblanks-- > 0)
    print_char(pft, ' ');
}

// Print a double
static void
print_double(struct pft *pft, long double num)
{
  char prefix[3], digits[128];
  unsigned long long ipart, fpart;
  int base, upper, i, precision, overflow;
  int nprefix, ndigits, nlzeros, nrzeros, nblanks;
  char *symbols;

  if ((pft->conversion == 'a') || (pft->conversion == 'A'))
    base = 16;
  else
    base = 10;

  precision = (pft->precision >= 0) ? pft->precision : 6;
  if (precision > 15) {
    nrzeros = precision - 15;
    precision = 15;
  } else {
    nrzeros = 0;
  }

  upper     = ((pft->conversion == 'A') || (pft->conversion == 'F'));
  symbols   = upper ? "0123456789ABCDEF" : "0123456789abcdef";

  ipart = (long long) num;
  num -= (long long) ipart;
  for (i = 0; i <= precision; i++)
    num *= base;
  fpart = (long long) num;
  if (num < 0) {
    ipart = -ipart;
    fpart = -fpart;
  }

  nprefix = ndigits = nlzeros = nblanks = 0;

  // Convert the alternate form prefix (store in reverse order).
  if (base == 16) {
    prefix[nprefix++] = upper ? 'X' : 'x';
    prefix[nprefix++] = '0';
  }

  // Convert the sign character.
  if (num < 0)
    prefix[nprefix++] = '-';
  else if (pft->flags & FLAG_SIGN)
    prefix[nprefix++] = '+';
  else if (pft->flags & FLAG_BLANK)
    prefix[nprefix++] = ' ';

  // Convert the digits (store in reverse order).
  ndigits = overflow = 0;
	do {
		digits[ndigits++] = symbols[fpart % 10];
	} while ((fpart /= 10) != 0);

	if (digits[0] >= symbols[base / 2]) {
		digits[0] = '0';
		overflow = 1;
		for (i = 1; i <= ndigits; i++)
			if (overflow) {
				overflow = (digits[i] == symbols[base - 1]);
				digits[i] = overflow ? '0' : digits[i] + 1;
			}
	}

  if (ndigits <= precision)
    nrzeros += precision - ndigits;

	if ((precision > 0) || (pft->flags & FLAG_ALT))
		digits[ndigits++] = '.';

  ipart += overflow;
  do {
    digits[ndigits++] = symbols[ipart % base];
  } while ((ipart /= base) != 0);

  // Determine the number of leading zeros and padding characters
  if (pft->width > (nprefix + ndigits + nrzeros - 1)) {
    if ((pft->flags & FLAG_ZERO) && !(pft->flags & FLAG_LEFT)) {
      nlzeros = pft->width - (nprefix + ndigits - nrzeros - 1);
      nblanks = 0;
    } else {
      nblanks = pft->width - (nprefix + ndigits - nrzeros - 1);
      nlzeros = 0;
    }
  }

  if (!(pft->flags & FLAG_LEFT))
    while (nblanks-- > 0)
      print_char(pft, ' ');

  while (nprefix > 0)
    print_char(pft, prefix[--nprefix]);
    
  while (nlzeros-- > 0)
      print_char(pft, '0');
  
  while (ndigits > 1)
    print_char(pft, digits[--ndigits]);

  while (nrzeros-- > 0)
      print_char(pft, '0');

  while (nblanks-- > 0)
    print_char(pft, ' ');
}

// Print a string.
static void
print_str(struct pft *pft, const char *s)
{
  int n, padding;
  
  n = strlen(s);
  if ((pft->precision > 0) && (pft->precision < n))
    n = pft->precision;
    
  padding = pft->width > n ? pft->width - n : 0;

  if (pft->flags & FLAG_LEFT)
    for ( ; n > 0; n--)
      print_char(pft, *s++);

  for ( ; padding > 0; padding--)
    print_char(pft, ' ');

  if (!(pft->flags & FLAG_LEFT))
    for ( ; n > 0; n--)
      print_char(pft, *s++);
}
