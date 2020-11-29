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
#include "threads/loader.h"
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
void close (int fd);
bool check_address (const char *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
//  printf ("sys num : %d\n", *(int *)f->esp);
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
    case 12:
    {
      int fd = *(int *)(f->esp + 4);
      close (fd);
      break; 
    }
  }
}

void
halt (void)
{
  shutdown_power_off ();
}

void 
exit (int status)
{
  discontinue_until_acquire_lock (&exit_lock);

  struct thread *t= thread_current ();
  char *file_name;
  char *args;
  file_name = strtok_r (t->name, " ", &args);
  printf ("%s: exit(%d)\n", file_name, status);

  /* Find the index of current thread and revise PMT. */
  int chd_idx = search_idx (t->tid);
  ASSERT (pmt[chd_idx].is_filled == true)
  pmt[chd_idx].ret_val = status;
  pmt[chd_idx].is_dead = true;

  /* Parent thread is kernel thread. */
  if (t->tid == 3)
  {
    thread_unblock (pmt[chd_idx].parent_thread);
    lock_release (&exit_lock);
    thread_exit (); 
  }

  struct thread *par = pmt[chd_idx].parent_thread;
  int par_idx = search_idx (par->tid);
  
  /* Close files which current thread is opening. */
  int i;
  for (i = 0; i < FDS_SIZE; i++)
    {
      int fmt_idx = search_fmt_idx (pmt[chd_idx].fds[i].fd);
      
      if (fmt_idx != -1)
        close (fmt[fmt_idx].fd);
    }

/*
  if (pmt[par_idx].waiting_child_cnt)
    {
      if (check_wcl (par_idx, t->tid))
        {
          pmt[par_idx].waiting_child_cnt--; 
          remove_wcl (par_idx, t->tid);
        }
      if (!pmt[par_idx].waiting_child_cnt)
        thread_unblock (par);
    }
*/
  if (pmt[par_idx].waiting_child_cnt)
    {
      pmt[par_idx].waiting_child_cnt--;
      remove_wcl (par_idx, t->tid);
      if (!pmt[par_idx].waiting_child_cnt)
        thread_unblock (par);
    }
  lock_release (&exit_lock);
  thread_exit ();
}

pid_t
exec (const char *cmd_line)
{
  discontinue_until_acquire_lock (&exec_lock);
  bool valid = check_address (cmd_line);
  if (!valid)
    {
      lock_release (&exec_lock);
      exit (-1);
    }
  int size = strlen (cmd_line);

  tid_t child_tid = process_execute (cmd_line);
  if (child_tid < 0)
    {
      lock_release (&exec_lock);
      return -1;
    }
  lock_release (&exec_lock); 
  return child_tid;
};

int 
wait (pid_t pid) 
{
  int idx = search_idx (pid);
  if (idx == -1)
    {
      return -1;
    }
  int result = process_wait (pid);
  
  /* Delete whole information of child process in process management table. */
  pmt[idx].is_filled = false;
  pmt[idx].thread_id = 0;
  memmove (pmt[idx].name, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16); 
  pmt[idx].ret_val = 0;
  ASSERT (pmt[idx].waiting_child_cnt == 0);
  pmt[idx].waiting_child_cnt = 0;
  pmt[idx].is_dead = false;
//  pmt[idx].parent_thread = NULL;
  int i = 0;
  while (i < WCL_SIZE)
    {
      pmt[idx].waiting_child_list[i] = 0;
      i++;
    }
  i = 0;
  while (i < FDS_SIZE)
    {
      pmt[idx].fds[i].fd = 0;
      pmt[idx].fds[i].file_ptr = NULL;
      i++;
    }
  return result;  
}

bool 
create (const char *file, unsigned initial_size)
{
  discontinue_until_acquire_lock (&create_lock);
  /* Check whether address of file is correct. */ 
  bool valid = check_address (file);
  if (!valid || file == NULL)
    {
      lock_release (&create_lock);
      exit (-1);
    }

  discontinue_until_acquire_lock (&filesys_lock);
  bool is_file_created = filesys_create (file, initial_size);  
  lock_release (&filesys_lock);
  lock_release (&create_lock);
  return is_file_created;
}

bool 
remove (const char *file)
{
  discontinue_until_acquire_lock (&remove_lock);

  /* Check whether address of file is correct. */ 
  bool valid = check_address (file);
  if (!valid || file == NULL)
    {
      lock_release (&remove_lock);
      exit (-1);
    }

  discontinue_until_acquire_lock (&filesys_lock);
  bool is_file_removed = filesys_remove (file);
  lock_release (&filesys_lock);
  lock_release (&remove_lock);
  return is_file_removed;
}

int 
open (const char *file) 
{
  discontinue_until_acquire_lock (&open_lock);

  /* Check whether address of file is correct. */ 
  bool valid = check_address (file);
  if (!valid || file == NULL)
  {
    lock_release (&open_lock);
    exit (-1);
  }

  int result;
  struct thread *t = thread_current ();

  int idx = search_idx (t->tid);
  ASSERT (idx != -1);
  
  int cfl_idx = search_cfl_empty ();

  discontinue_until_acquire_lock (&filesys_lock);
  struct file *file_ = filesys_open (file);
  lock_release (&filesys_lock);
  if (file_ == NULL)
    {
      lock_release (&open_lock);
      return -1;
    }
  fd_cnt++;
  
  /* Fill process management table. */
  int fds_idx = search_fds_empty (idx);
  pmt[idx].fds[fds_idx].fd = fd_cnt + 1;
  pmt[idx].fds[fds_idx].file_ptr = file_;
  result = fd_cnt + 1;

  /* Fill current file list and link fmt with cfl. */
  cfl[cfl_idx].is_filled = true;
  cfl[cfl_idx].is_opened = true;
  cfl[cfl_idx].file_ptr = file_;
  insert_cfl_fd (fd_cnt + 1, file_);
  int i;
  for (i = 0; i < 16; ++i)
    {
      if (thread_current ()->name[i] == '\0')
        break;
    }
  i = 0;
  for (i = 0; i < 16; ++i)
    {
      cfl[cfl_idx].file_name[i] = file[i];
      if (file[i] == '\0')
        break;
    }
  
  /* Fill file mapping table. */
  int fmt_idx = search_fmt_empty ();
  fmt[fmt_idx].fd = fd_cnt + 1;
  fmt[fmt_idx].cfl_idx = cfl_idx;
  fmt[fmt_idx].pmt_idx = idx;  

  lock_release (&open_lock);
  return result;
}

int
filesize (int fd)
{
  discontinue_until_acquire_lock (&filesize_lock);
  int fmt_idx = search_fmt_idx (fd);
  ASSERT (fmt_idx >= 0 && fmt_idx < FMT_SIZE);
  int cfl_idx = fmt[fmt_idx].cfl_idx;

  lock_release (&filesize_lock);
  return file_length (cfl[cfl_idx].file_ptr);
}

int 
read (int fd, void *buffer, unsigned size)
{
  discontinue_until_acquire_lock (&read_lock);
  bool valid = check_address (buffer);
  if (!valid || buffer == NULL || fd == 1)
    {
      lock_release (&read_lock);
      exit (-1);
    }

  if (fd == 0)
    {
      off_t bytes_read = input_getc ();
      lock_release (&read_lock);
      return bytes_read;
    }

  int fmt_idx = search_fmt_idx (fd);
  if (fmt_idx == -1)
    {
      lock_release (&read_lock);
      return -1;
    }
  int cfl_idx = fmt[fmt_idx].cfl_idx;
//  printf ("tid : %d  before read pos : %d\n", thread_current ()->tid,  file_tell(cfl[cfl_idx].file_ptr));
  off_t bytes_read = file_read (cfl[cfl_idx].file_ptr, buffer, size); 
//  printf ("tid : %d  after read pos : %d\n", thread_current ()->tid, file_tell (cfl[cfl_idx].file_ptr));
  lock_release (&read_lock);
  return bytes_read;
}

int
write (int fd, const void *buffer, unsigned size)
{
  discontinue_until_acquire_lock (&write_lock);
  bool valid = check_address (buffer);
  if (!valid || buffer == NULL || fd == 0)
    {
      lock_release (&write_lock);
      exit (-1);
    }

  if (fd == 1)
    {
      putbuf(buffer, size);
      lock_release (&write_lock);
      return size;
    }
  
  int fmt_idx = search_fmt_idx (fd);
  if (fmt_idx == -1)
    {
      lock_release (&write_lock);
      return -1;
    }
  int cfl_idx = fmt[fmt_idx].cfl_idx;

  /* Exit if user try to write on ELF file which is running. */
  int i = 0;
  while (true)
    {
      if (!strcmp (pmt[i].name, cfl[cfl_idx].file_name))
        {
          lock_release (&write_lock);
          return 0;
        }
      i++;
      if (i >= PMT_SIZE)
        break;
    }

  off_t bytes_written = file_write (cfl[cfl_idx].file_ptr, buffer, size);
  lock_release (&write_lock);
  return bytes_written;
}

void 
seek (int fd, unsigned position) 
{
  discontinue_until_acquire_lock (&seek_lock);
  int fmt_idx = search_fmt_idx (fd);
  ASSERT (fmt_idx >= 0 && fmt_idx < FMT_SIZE);
  int cfl_idx = fmt[fmt_idx].cfl_idx;

  lock_release (&seek_lock);
  file_seek (cfl[cfl_idx].file_ptr, position);
}

unsigned 
tell (int fd) 
{
  discontinue_until_acquire_lock (&tell_lock);
  int fmt_idx = search_fmt_idx (fd);
  ASSERT (fmt_idx >= 0 && fmt_idx < FMT_SIZE);
  int cfl_idx = fmt[fmt_idx].cfl_idx;

  lock_release (&tell_lock);
  return file_tell (cfl[cfl_idx].file_ptr);
}

void
close (int fd)
{
  discontinue_until_acquire_lock (&close_lock);
  int fmt_idx = search_fmt_idx (fd);
  if (fmt_idx == -1)
    {
      lock_release (&close_lock);
      exit (-1);
    }
  int cfl_idx = fmt[fmt_idx].cfl_idx;
  int idx = fmt[fmt_idx].pmt_idx;
  ASSERT (cfl_idx >= 0 && cfl_idx < CFL_SIZE);
  ASSERT (idx >= 0 && idx < PMT_SIZE);

  if (pmt[idx].thread_id != thread_current ()->tid)
    {
      lock_release (&close_lock);
      return;
    }

  struct file *file = cfl[cfl_idx].file_ptr;

//  if (!lock_held_by_current_thread (&cfl[cfl_idx].file_lock))
//    return;
  if (cfl[cfl_idx].is_opened == true)
  {
    int fds_idx = search_fds_idx (idx, fd);
      if (fds_idx == -1)
        {
          lock_release (&close_lock);
          return;
        }
    file_close (file);
    remove_cfl_fd (fd, file);
    cfl[cfl_idx].is_opened = false;
  }
  lock_release (&close_lock);
}

bool
check_address (const char *addr)
{
  void *low_limit = (void *) 0x8048000;
  void *high_limit = (void *) 0xc0000000;
  if ((void *) addr >= low_limit && (void *) addr < high_limit)
    return true;
  return false;
}
