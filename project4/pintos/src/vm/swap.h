#ifndef __VM_SWAP_H
#define __VM_SWAP_H

#define SECTOR_NUM 8192
#define SLOT_SIZE 8

#include "devices/block.h"
#include <bitmap.h>
#include "vm/frame.h"

struct block *swap;
struct bitmap *swap_table;

struct slot_management_table
	{
		struct page *page;
		size_t slot_idx;
	};
struct slot_management_table smt[PMEM_SIZE];

/* Functions related with swap disk. */
void swap_init (void);
void swap_write (size_t slot, const void *buffer);
void swap_read (size_t slot, void *buffer);

/* Functions related with swap table. */
void swap_table_init (void);
bool swap_table_test (size_t idx);
void swap_slot_set (size_t start, bool value);
size_t swap_slot_scan (bool value);
bool swap_slot_test (size_t slot_start, bool value);

/* Functions related with slot management table. */
int swap_smt_empty (void);

#endif
