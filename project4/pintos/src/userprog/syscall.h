#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "filesys/filesys.h"
#include "filesys/file.h"
#include <string.h>

typedef int mapid_t;

mapid_t map_cnt;

struct removed_list
	{
		bool is_filled;
		char name[16];
	};
struct removed_list removed_list[5];

void syscall_init (void);
void exit (int);

#endif /* userprog/syscall.h */

