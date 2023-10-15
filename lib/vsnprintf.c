#include <stdio.h>

struct ctx {
  char   *s;
  size_t  count;
  size_t  max;
};

static void
sputc(void *arg, int ch)
{
  struct ctx *ctx = (struct ctx *) arg;

  if (ctx->count < (ctx->max - 1))
    ctx->s[ctx->count++] = ch;
}

int
vsnprintf(char *s, size_t n, const char *format, va_list ap)
{
  struct ctx ctx = {
    .s     = s,
    .count = 0,
    .max   = n,
  };

  xprintf(sputc, &ctx, format, ap);

  ctx.s[ctx.count] = '\0';

  return ctx.count;
}
