#ifndef __VM_FRAME_H
#define __VM_FRAME_H

#define F_REF 0x1

#include <list.h>

int free_cnt;
struct list p_mem;
struct frame
	{
		void *addr;
		struct list_elem list_elem;
	};


#endif
