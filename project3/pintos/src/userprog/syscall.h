#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "filesys/filesys.h"
#include "filesys/file.h"
#include <string.h>

typedef int mapid_t;

mapid_t map_cnt;

void syscall_init (void);
void exit (int);

#endif /* userprog/syscall.h */

