#ifndef __VM_PAGE_H
#define __VM_PAGE_H
#include <hash.h>
#include "filesys/off_t.h"
#include <stdint.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"

enum type
	{
		NORMAL,
		MMAP,
		SWAP
	};

struct vpage							/* virtual page. */
	{
		char name[16];
//		struct file *file;
		bool is_load;
		bool writable;
		void *vaddr;
		void *paddr;
		uint32_t page_read_bytes;
		uint32_t page_zero_bytes;
		off_t off;						/* file offset. */
		struct hash_elem h_elem;
		struct list_elem m_elem;
		enum type type;
	};

struct mmap_entry
	{
		struct file *file;
		struct list_elem mmap_elem;
		struct list mmap_list;
		mapid_t mapping;
		unsigned short remain_cnt;
	};
		
		
unsigned vpage_hash (const struct hash_elem *v_, void *aux);
bool vpage_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
void vpage_action (struct hash_elem *e, void *aux);
struct vpage *vpage_lookup (struct hash *vmhash, const void *addr);

#endif		
