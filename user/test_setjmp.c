// test <setjmp.h> functions
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

static int count;
static jmp_buf buf0;

// Get the current stack pointer value (assuming ch is allocated on the stack).
// This is useful to test for stack creep, i.e. the condition when we fail to
// restore the stack state.
static char *
stackptr(void)
{
  char ch;
  return &ch;
}

static int
try_it(void)
{
  jmp_buf buf1;
  char *sp = stackptr();

  count = 0;
  switch (setjmp(buf0)) {
  case 0:
    assert(sp == stackptr());
    assert(count == 0);
    count++;
    longjmp(buf0, 0);   // Should jump to 1 (special case if val is 0)
    break;

  case 1:
    assert(sp == stackptr());
    assert(count == 1);
    count++;
    longjmp(buf0, 2);   // Should jump to 2
    break;

  case 2:
    assert(sp == stackptr());
    assert(count == 2);
    count++;

    // Test nesting
    switch (setjmp(buf1)) {
    case 0:
      assert(sp == stackptr());
      assert(count == 3);
      count++;
      longjmp(buf1, -7);    // Should jump to -7
      break;
    case -7:
      assert(sp == stackptr());
      assert(count == 4);
      count++;
      longjmp(buf0, 3);     // Should jump to 3 in the outer switch
      break;
    case 5:
      return 42;
    default:
      return 0;
    }
    break;

  case 3:
    longjmp(buf1, 5);   // Should jump to 5 in the nested switch
    break;
  }
  return -1;
}

int
main(void)
{
  assert(try_it() == 42);
  printf("sizeof (jmp_buf) = %u\n", sizeof(jmp_buf));
  printf("SUCCESS testing <setjmp.h>\n");
  return 0;
}
