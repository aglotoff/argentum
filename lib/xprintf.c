#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct ctx {
  void  (*xputc)(void *, int);
  void   *xputc_arg;
  char    conversion;
  int     length;
  int     flags;
  int     width;
  int     precision;
};

enum {
  FLAG_LEFT  = (1 << 0),  // Left-justified
  FLAG_SIGN  = (1 << 1),  // Always display the sign character
  FLAG_BLANK = (1 << 2),  // Print space if there is no sign character
  FLAG_ALT   = (1 << 3),  // Alternate form
  FLAG_ZERO  = (1 << 4),  // Print leading zeros
};

static long long          arg_int(va_list *, int);
static unsigned long long arg_uint(va_list *, int);

static void               print_char(struct ctx *, char);
static void               print_int(struct ctx *, long long);
static void               print_str(struct ctx *, const char *);

/**
 * Generic function to print formatted data.
 *
 * @param xputc     Pointer to the output function
 * @param xputc_arg The first argument for the output function
 * @param format    The format string
 * @param ap        A variable argument list
 */
void
xprintf(void            (*xputc)(void *, int),
        void             *xputc_arg,
        const char       *format,
        va_list ap)
{
  struct ctx ctx = {
    .xputc     = xputc,
    .xputc_arg = xputc_arg,
  };

  long long num;
  char *str;

  while (*format != '\0') {
    ctx.width = ctx.precision = -1;
    ctx.length = 0;

    // Print literally
    while ((*format != '\0') && (*format != '%'))
      print_char(&ctx, *format++);

    if (*format == '\0')
      break;

    format++;

    // Flags
    ctx.flags = 0;
    while (strchr("-+ #0", *format)) {
      switch (*format++) {
      case '-':
        ctx.flags |= FLAG_LEFT; break;
      case '+':
        ctx.flags |= FLAG_SIGN; break;
      case ' ':
        ctx.flags |= FLAG_BLANK; break;
      case '#':
        ctx.flags |= FLAG_ALT; break;
      case '0':
        ctx.flags |= FLAG_ZERO; break;
      }
    }

    // Field width
    ctx.width = 0;
    if (*format == '*') {
      ctx.width = va_arg(ap, unsigned);
      if (ctx.width < 0) {
        // Same as '-' flag.
        ctx.width = -ctx.width;
        ctx.flags |= FLAG_LEFT; break;
      }

      format++;
    } else if ((*format >= '0') && (*format <= '9')) {
      while ((*format >= '0') && (*format <= '9')) {
        ctx.width = ctx.width * 10 + (*format - '0');
        format++;
      }
    }

    // Field precision
    ctx.precision = 0;
    if (*format == '.') {
      format++;

      if (*format == '*') {
        ctx.precision = va_arg(ap, int);
        format++;
      } else {
        while ((*format >= '0') && (*format <= '9')) {
          ctx.precision = ctx.precision * 10 + (*format - '0');
          format++;
        }
      }
    }

    // Length modifier
    ctx.length = 0;
    while ((*format == 'l') || (*format == 'h')) {
      switch (*format) {
      case 'h':
        ctx.length--;
        break;
      case 'l':
      case 'L':
        ctx.length++;
        break;
      }
      format++;
    }

    // Do conversion
    ctx.conversion = *format;
    switch (ctx.conversion) {
      // Signed decimal
      case 'd':
      case 'i':
        num = arg_int(&ap, ctx.length);
        print_int(&ctx, num);
        break;

      // Unsigned decimal, octal, hexadecimal
      case 'u':
      case 'o':
      case 'O':
      case 'x':
      case 'X':
        num = arg_uint(&ap, ctx.length);
        print_int(&ctx, num);
        break;

      // Pointer
      case 'p':
        ctx.width = sizeof(uintptr_t) * 2 + 2;
        ctx.flags |= FLAG_ZERO | FLAG_ALT;
        num = (uintptr_t) va_arg(ap, void *);
        print_int(&ctx, num);
        break;

      // Character
      case 'c':
        num = va_arg(ap, int);
        print_char(&ctx, (unsigned char) num);
        break;

      // C string
      case 's':
        if (!(str = va_arg(ap, char *)))
          str = "(null)";
        print_str(&ctx, str);
        break;

      // A % character
      case '%':
        print_char(&ctx, '%');
        break;

      // Unknown conversion specifier, print the whole sequence literally
      default:
        while (*format != '%')
          format--;
        break;
    }

    format++;
  }
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions to fetch var args
 * ----------------------------------------------------------------------------
 */

// Take the next integer argument of a given length from the variable argument
// list
static long long
arg_int(va_list *ap, int length)
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
arg_uint(va_list *ap, int length)
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

/*
 * ----------------------------------------------------------------------------
 * Helper functions to print the conversion results
 * ----------------------------------------------------------------------------
 */

// Print a single character keeping track of the number of characters printed
// so far
static void
print_char(struct ctx *ctx, char c)
{
  ctx->xputc(ctx->xputc_arg, c);
}

// Print a formatted integer.
static void
print_int(struct ctx *ctx, long long num)
{
  char prefix[3], digits[64];
  unsigned long long abs_num;
  int base, sign, upper;
  int nprefix, ndigits, nzeros, nblanks;
  char *symbols;

  if (strchr("oO", ctx->conversion))
    base = 8;
  else if (strchr("xXp", ctx->conversion))
    base = 16;
  else
    base = 10;

  sign    = ((ctx->conversion == 'd') || (ctx->conversion == 'i'));
  upper   = ((ctx->conversion == 'O') || (ctx->conversion == 'X'));
  symbols = upper ? "0123456789ABCDEF" : "0123456789abcdef";

  abs_num = num;
  if (sign && (num < 0))
    abs_num = -abs_num;

  nprefix = ndigits = nzeros = nblanks = 0;

  // Convert the alternate form prefix (store in reverse order).
  if (ctx->flags & FLAG_ALT) {
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
  else if (ctx->flags & FLAG_SIGN)
    prefix[nprefix++] = '+';
  else if (ctx->flags & FLAG_BLANK)
    prefix[nprefix++] = ' ';

  // Convert the digits (store in reverse order).
  do {
    digits[ndigits++] = symbols[abs_num % base];
  } while ((abs_num /= base) != 0);

  // Determine the number of leading zeros and padding characters
  if (ctx->precision > ndigits)
    nzeros = ctx->precision - ndigits;
  if (ctx->width > (nprefix + nzeros + ndigits)) {
    if ((ctx->flags & FLAG_ZERO) && !(ctx->flags & FLAG_LEFT)) {
      nzeros  = ctx->width - (nprefix + ndigits);
      nblanks = 0;
    } else {
      nblanks = ctx->width - (nprefix + nzeros + ndigits);
    }
  }

  if (!(ctx->flags & FLAG_LEFT))
    while (nblanks-- > 0)
      print_char(ctx, ' ');

  while (nprefix > 0)
    print_char(ctx, prefix[--nprefix]);
    
  while (nzeros-- > 0)
      print_char(ctx, '0');
  
  while (ndigits > 0)
    print_char(ctx, digits[--ndigits]);

  while (nblanks-- > 0)
    print_char(ctx, ' ');
}

// Print a string.
static void
print_str(struct ctx *ctx, const char *s)
{
  int n, padding;
  
  n = strlen(s);
  if ((ctx->precision > 0) && (ctx->precision < n))
    n = ctx->precision;
    
  padding = ctx->width > n ? ctx->width - n : 0;

  if (ctx->flags & FLAG_LEFT)
    for ( ; n > 0; n--)
      print_char(ctx, *s++);

  for ( ; padding > 0; padding--)
    print_char(ctx, ' ');

  if (!(ctx->flags & FLAG_LEFT))
    for ( ; n > 0; n--)
      print_char(ctx, *s++);
}
