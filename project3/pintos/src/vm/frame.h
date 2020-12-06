#ifndef __VM_FRAME_H
#define __VM_FRAME_H

#define PMEM_SIZE 383

#include <list.h>
#include "threads/thread.h"
#include "vm/page.h"

int free_cnt;

struct page
	{
		struct vpage *vpage;
		struct thread *thread;
		struct list_elem list_elem;
	};

struct list p_mem;

void frame_fill (struct vpage *vpage); 
void frame_remove (struct vpage *vpage);



#endif
