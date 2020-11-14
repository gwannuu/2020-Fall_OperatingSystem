#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "filesys/off_t.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include <string.h>
#include "userprog/pagedir.h"
#include <devices/input.h>
#include "filesys/directory.h"
static void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)  /* UNUSED deleted. */ 
{
  //printf("sys num : %d\n", *(int *)(f->esp));
  struct thread *t = thread_current ();
  void *pd = pagedir_get_page (t->pagedir, f->esp);
  if (!pd)
    exit (-1);
  switch (*(int *)(f->esp))
  {
    case 0:
    {
      halt ();
      break;
    }
    case 1:
    {
      if (f->esp + 4 >= PHYS_BASE)
        exit (-1);
      int status = *(int *)(f->esp + 4);
      exit (status);
      break;
    }
    case 2:
    {
      const char *cmd_line = *(const char **)(f->esp + 4); 
      f->eax = exec (cmd_line);
      break;
    }
    case 3:
    {
      pid_t pid = *(int *)(f->esp + 4);
      f->eax = wait (pid);
      break;
    }
    case 4:
    {
      const char *file = *(const char **)(f->esp + 16);
      unsigned initial_size = *(unsigned *)(f->esp + 20);
      f->eax = create (file, initial_size);
      break;
    }
    case 5:
    {
      const char *file = *(const char **)(f->esp + 4);
      f->eax = remove (file);
      break;
    }
    case 6:
    {
      const char *file = *(const char **)(f->esp + 4);
      f->eax = open (file);
      break;
    }
    case 7:
    {
      int fd = *(int *)(f->esp + 4);
      f->eax = filesize (fd);
      break;
    }
    case 8:
    {
      int fd = *(int *)(f->esp + 20);
      void *buffer = *(char **)(f->esp + 24);
      unsigned size = *(unsigned *)(f->esp + 28);
      f->eax = read (fd, buffer, size);
      break;
    }
    case 9:
    {
      int fd = *(int *)(f->esp + 20);
      const void *buffer = *(const char **)(f->esp + 24);
      unsigned size = *(unsigned *)(f->esp + 28);
      f->eax = write (fd, buffer, size);
      break;
    }
    case 10:
    {
      int fd = *(int *)(f->esp + 16);
      unsigned position = *(unsigned *)(f->esp + 20);
      seek (fd, position);
      break;
    }
    case 11:
    {
      int fd = *(int *)(f->esp + 4);
      f->eax = tell (fd);
      break;
    }
  }
}

void
halt (void)
{
  shutdown_power_off ();
}

/* Return status to kernel, But how can? */
void 
exit (int status)
{
  struct thread *t= thread_current ();
  tid_t cur_tid = t->tid;
  char *file_name;
  char *args;
  file_name = strtok_r (t->name, " ", &args);
  printf ("%s: exit(%d)\n", file_name, status);

  int idx = search_idx (cur_tid);
  ASSERT (pmt[idx].is_filled == true)
  pmt[idx].ret_val = status;
  thread_unblock (pmt[idx].parent_thread);
  
  thread_exit ();
}

pid_t
exec (const char *cmd_line)
{
  char *args;
  int size = strlen (cmd_line);
  char cmd_copy[size];
  strlcpy (cmd_copy, cmd_line, size + 1);
//  char *file_name = strtok_r (cmd_copy, " ", &args);

//  struct dir *dir = dir_open_root ();
/*  struct inode *inode = NULL;
  if (!dir_lookup (dir, file_name, &inode))
  {
    filesys_open (file_name);
    printf("not exist\n");
    dir_close (dir);
    return -1;
  }
  dir_close (dir);*/

  tid_t child_tid = process_execute (cmd_line);
  if (child_tid < 0)
    return -1;
  return child_tid;
};

int 
wait (pid_t pid) 
{
  int idx = search_idx (pid);
  if (idx == -1)
    return -1;
/*
  int result = process_wait (pid);
  
  struct thread *t = thread_current ();
  idx = search_idx (t->tid);
  pmt[idx].is_waiting_child = false;
  return result;
*/
  return process_wait (pid);
}

bool 
create (const char *file, unsigned initial_size)
{
  if (file == NULL)
    return false;
  bool is_file_created = filesys_create (file, initial_size);  
  return is_file_created;
}

bool 
remove (const char *file)
{
  if (file == NULL)
    return false;
  bool is_file_removed = filesys_remove (file);
  return is_file_removed;
}

int 
open (const char *file) 
{
  if (file == NULL)
    return -1;
  
  struct thread *t = thread_current ();
  struct file *file_ = filesys_open (file);
  if (file_ == NULL)
    return -1;

  fd_cnt++;
  int idx = search_idx (t->tid);
  ASSERT (idx != -1);
  int fds_idx = search_fds_blank (idx);
  pmt[idx].fds[fds_idx].is_filled = true;
  pmt[idx].fds[fds_idx].fd = fd_cnt + 1; 
  pmt[idx].fds[fds_idx].file_ptr = file_;
  return pmt[idx].fds[fds_idx].fd;
}

int
filesize (int fd)
{
  struct thread *t = thread_current ();
  int idx = search_idx (t->tid);
  int fds_idx = search_fds_idx (idx, fd);
  if (fds_idx == -1)
    return -1;

  struct file *file = pmt[idx].fds[fds_idx].file_ptr;
  return file_length (file);
}

int 
read (int fd, void *buffer, unsigned size)
{
  if (fd == 0)
  {
    off_t bytes_read = input_getc ();
    return bytes_read;
  }
  
  struct thread *t = thread_current ();
  int idx = search_idx (t->tid);
  int fds_idx = search_fds_idx (idx, fd);
  if (fds_idx == -1)
    return -1;

  struct file *file_ = pmt[idx].fds[fds_idx].file_ptr;
  off_t bytes_read = file_read (file_, buffer, size); 
  return bytes_read;
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (buffer == NULL)
    return -1;
 
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  
  struct thread *t = thread_current ();
  int idx = search_idx (t->tid);
  int fds_idx = search_fds_idx (idx, fd);
  if (fds_idx == -1)
    return -1;
  
  struct file *file = pmt[idx].fds[fds_idx].file_ptr;
  off_t bytes_written = file_write (file, buffer, size);
  return bytes_written;
}

void 
seek (int fd, unsigned position) 
{
  struct thread *t = thread_current ();
  int idx = search_idx (t->tid);
  int fds_idx = search_fds_idx (idx, fd);
  struct file *file = pmt[idx].fds[fds_idx].file_ptr;
  file_seek (file, position);
}

unsigned 
tell (int fd) 
{
  struct thread *t = thread_current ();
  int idx = search_idx (t->tid);
  int fds_idx = search_fds_idx (idx, fd);
  struct file *file = pmt[idx].fds[fds_idx].file_ptr;
  return file_tell (file);
}

