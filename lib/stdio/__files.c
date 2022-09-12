#include <stdio.h>

// Standard input stream
static FILE stdin_file = {
  .mode = _MODE_READ,
  .fd   = 0,
};

// Standard output stream
static FILE stdout_file = {
  .mode = _MODE_WRITE,
  .fd   = 1,
};

// Standard error output stream
static FILE stderr_file = {
  .mode = _MODE_WRITE,
  .fd   = 2,
};

FILE *__files[FOPEN_MAX] = {
  &stdin_file,
  &stdout_file,
  &stderr_file,
};
