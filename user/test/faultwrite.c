// Buggy program - try to write to address 0.
int
main(void)
{
  *(int *) 0x0 = 0x12345678;
  return 0;
}
