#include "vm/swap.h"
#include "devices/block.h"
#include <bitmap.h>
#include <debug.h>

/* Functions related with swap disk. */
void
swap_init (void)
{
  swap = block_get_role (BLOCK_SWAP);
}

void 
swap_write (size_t slot, const void *buffer)
{
  /* from buffer to block. */
  int i;
  for (i = 0; i < SLOT_SIZE; i++)
    block_write (swap, slot + i, buffer);
}

void 
swap_read (size_t slot, void *buffer)
{
  /* from block to buffer. */ 
  int i;
  for (i = 0; i < SLOT_SIZE; i++)
    block_read (swap, slot + i, buffer);
}

/* Functions related with swap table. */
void
swap_table_init (void)
{
  swap_table = bitmap_create (SECTOR_NUM);
}

bool
swap_table_test (size_t idx)
{
  return bitmap_test (swap_table, idx);
}

void
swap_slot_set (size_t slot_start, bool value)
{
  size_t start = slot_start * SLOT_SIZE;
  bitmap_set_multiple (swap_table, start, SLOT_SIZE, value);
}

size_t
swap_slot_scan (bool value)
{
  size_t result = bitmap_scan (swap_table, 0, bitmap_size (swap_table), value);
  if (result == BITMAP_ERROR)
    PANIC ("swap table is unstable!\n" );
  return result / SLOT_SIZE;
}

bool
swap_slot_test (size_t slot_start, bool value)
{
  size_t start = slot_start * SLOT_SIZE;
  return SLOT_SIZE == bitmap_count (swap_table, start, SLOT_SIZE, value);
}

/* Functions related with slot management table. */

int swap_smt_empty (void)
{
  int i;
  for (i = 0; i < PMEM_SIZE; ++i)
    {
      if (smt[i].page == NULL)
        return i;
    }
  return -1;
}

