// Buggy program - try to write to the kernel address space.
int
main(void)
{
  *(int *) 0x80010000 = 0x12345678;
  return 0;
}
