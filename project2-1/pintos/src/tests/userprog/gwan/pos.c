#include <syscall.h>
#include <stdio.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  int handle = open ("poem.txt");
  msg (handle);
  int tell = tell (handle);
  msg (tell);
}
