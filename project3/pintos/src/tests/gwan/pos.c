#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

int
test_main (void)
{
  int handle = open ("poem.txt");
  msg ("next pos : %d\n", tell ("poem.txt"));
  return 35;
}
  
