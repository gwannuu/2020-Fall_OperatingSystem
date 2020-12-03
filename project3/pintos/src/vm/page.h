#ifndef __VM_PAGE_H
#define __VM_PAGE_H
#include <hash.h>
#include "filesys/off_t.h"
#include <stdint.h>
#include "filesys/file.h"
#include "filesys/filesys.h"

struct vpage							/* virtual page. */
	{
		char name[16];
		bool is_load;
		bool writable;
		void *vaddr;
		uint32_t page_read_bytes;
		uint32_t page_zero_bytes;
		off_t off;						/* file offset. */
		struct hash_elem h_elem;
	};
		
unsigned vpage_hash (const struct hash_elem *v_, void *aux);
bool vpage_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);

#endif		
