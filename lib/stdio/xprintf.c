#include <stdio.h>
#include <stdint.h>
#include <string.h>

static long long          get_int_arg(va_list *, int);
static unsigned long long get_uint_arg(va_list *, int);
static void               print_num(void (*)(void *, int),
                                    void *,
                                    long long,
                                    int,
                                    int,
                                    int,
                                    char);
static void               print_str(void (*)(void *, int),
                                    void *,
                                    const char *,
                                    int,
                                    int);

/**
 * Generic function to print formatted data.
 * 
 * This is a simplified printf-like implementation that supports the following
 * subset of features:
 * - Conversion specifiers: d, u, o, x, p, c, s
 * - Length modifiers: hh, h, l, ll
 * - Width and precision fields
 *
 * @param xputc Pointer to the output function.
 * @param xputc_arg The first argument for the output function.
 * @param format The format string.
 * @param ap A variable argument list.
 */
void 
xprintf(void      (*xputc)(void *, int),
        void       *xputc_arg,
        const char *format,
        va_list     ap)
{
  for (;;) {
    int length, width, precision;
    char padc;
    char *str;
    long long num;

    // Print literally
    while (*format && (*format != '%')) {
      xputc(xputc_arg, *format++);
    }

    if (*format == '\0') {
      va_end(ap);
      break;
    }
    format++;

    // Padding character
    padc = ' ';
    if (*format == '0') {
      padc = *format++;
    }

    // Field width
    width = 0;
    if (*format == '*') {
      format++;
      width = va_arg(ap, unsigned);
    } else {
      while ((*format >= '0') && (*format <= '9')) {
        width = width * 10 + (*format - '0');
        format++;
      }
    }

    // Field precision
    precision = 0;
    if (*format == '.') {
      format++;
      if (*format == '*') {
        format++;
        precision = va_arg(ap, int);
      } else {
        while ((*format >= '0') && (*format <= '9')) {
          precision = precision * 10 + (*format - '0');
          format++;
        }
      }
    }

    // Length modifier
    length = 0;
    while ((*format == 'l') || (*format == 'h')) {
      switch (*format) {
      case 'h':
        length--;
        break;
      case 'l':
        length++;
        break;
      }
      format++;
    }

    // Do conversion
    switch (*format) {
      case 'd':
        num = get_int_arg(&ap, length);
        print_num(xputc, xputc_arg, num, 10, width, 1, padc);
        break;

      case 'u':
        num = get_uint_arg(&ap, length);
        print_num(xputc, xputc_arg, num, 10, width, 0, padc);
        break;

      case 'o':
        num = get_uint_arg(&ap, length);
        print_num(xputc, xputc_arg, num, 8, width, 0, padc);
        break;
      
      case 'p':
        width = sizeof(uintptr_t) * 2;
        length = 1;
        padc = '0';
        xputc(xputc_arg, '0');
        xputc(xputc_arg, 'x');
        // fall through

      case 'x':
        num = get_uint_arg(&ap, length);
        print_num(xputc, xputc_arg, num, 16, width, 0, padc);
        break;

      case 'c':
        num = get_int_arg(&ap, -2);
        xputc(xputc_arg, (char) num);
        break;

      case 's': {
        if (!(str = va_arg(ap, char *))) {
          str = "(null)";
        }
        print_str(xputc, xputc_arg, str, width, precision);
        break;   
      }
      
      case '%':
        xputc(xputc_arg, '%');
        break;

      default:
        // Unknown conversion specifier, print the whole sequence literally
        while (*format != '%') {
          format--;
        }
        break;
    }

    format++;
  }
}

/**
 * Take the next integer argument of a given length from the variable argument
 * list.
 */
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

/*
 * Take the next unsigned integer argument of a given length from the variable
 * argument list.
 */
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

/**
 * Print a formatted integer.
 */
static void
print_num(void     (*xputc)(void *, int),
          void      *xputc_arg, 
          long long  num,
          int        base,
          int        width,
          int        sign,
          char       padc)
{
  static const char *const digits = "0123456789abcdef";
  char buf[65];   // 64 = max integer size in binary + an optional sign
  unsigned long long abs_num;
  int i;

  abs_num = num;
  if (sign && (num < 0)) {
    abs_num = -abs_num;
  }

  i = 0;
  do {
    buf[i++] = digits[abs_num % base];
  } while ((abs_num /= base) != 0);

  if (sign && (num < 0)) {
    buf[i++] = '-';
  }

  while (width > i) {
    xputc(xputc_arg, padc);
    width--;
  }

  while (i > 0) {
    xputc(xputc_arg, buf[--i]);
  }
}

/*
 * Print a string.
 */
static void
print_str(void      (*xputc)(void *, int),
          void       *xputc_arg, 
          const char *s,
          int         width,
          int         precision)
{
  int n = strlen(s);

  if ((precision > 0) && (precision < n)) {
    n = precision;
  }

  for ( ; width > n; width--) {
    xputc(xputc_arg, ' ');
  }

  while (n > 0) {
    xputc(xputc_arg, *s++);
    n--;
  }
}
