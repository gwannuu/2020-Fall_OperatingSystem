#include "vm/frame.h"
#include <debug.h>

bool
frame_is_full (void)
{
  int i = 0;
  int count = 0;
  while (true)
    {
      if (p_mem[i].is_filled)
        {
          count++;
          if (i >= PMEM_SIZE)
            break;
        }
    }
  return count == PMEM_SIZE;
}

bool
frame_fill (void *addr)
{
  if (frame_is_same_exist (addr) != -1)
    PANIC ("Same address is already on physical memory!\n");
  int i;
  for (i = 0; i < PMEM_SIZE; i++)
    {
      if (!p_mem[i].is_filled)
        {
          p_mem[i].is_filled = true;
          p_mem[i].addr = addr;
          return true;
        }
    }
  return false;
}

bool
frame_remove (void *addr)
{
  int i;
  for (i = 0; i < PMEM_SIZE; i++)
    {
      if (p_mem[i].addr == addr)
        {
          p_mem[i].is_filled = false;
          p_mem[i].addr = NULL;
          return true;
        }
    }
  return false;
}

int
frame_is_same_exist (void *addr)
{
  int i;
  for (i = 0; i < PMEM_SIZE; i++)
    {
      if (p_mem[i].addr == addr)
        return i;
    }
  return -1;
}
