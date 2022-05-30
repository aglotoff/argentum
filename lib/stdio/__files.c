#include <stdio.h>

// Standard input stream
static FILE stdin_file = {
  ._Mode   = _MODE_READ,
  ._Fildes = 0,
};

// Standard output stream
static FILE stdout_file = {
  ._Mode   = _MODE_WRITE,
  ._Fildes = 1,
};

// Standard error output stream
static FILE stderr_file = {
  ._Mode   = _MODE_WRITE,
  ._Fildes = 2,
};

FILE *__files[FOPEN_MAX] = {
  &stdin_file,
  &stdout_file,
  &stderr_file,
};
