#include <iostream>
#include <sys/mman.h>

int
main()
{
  std::cout << "Hello world" << std::endl;
  mmap(NULL, 0, 0, 0, 0, 0);
  return 0;
}
