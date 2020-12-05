#ifndef __VM_FRAME_H
#define __VM_FRAME_H

#define PMEM_SIZE 383
#define F_REF 0x1

#include <list.h>

int free_cnt;
struct physical_memory
	{
		bool is_filled;
		void *addr;
	};
struct physical_memory p_mem[PMEM_SIZE];	

bool frame_is_full (void);
bool frame_fill (void *addr); 
bool frame_remove (void *addr);
int frame_is_same_exist (void *addr);



#endif
