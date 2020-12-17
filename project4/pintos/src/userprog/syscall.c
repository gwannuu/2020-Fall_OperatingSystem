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

#include "threads/malloc.h"
#include "vm/page.h"
#include "threads/pte.h"
#include "vm/frame.h"

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
mapid_t mmap (int fd, void *addr);
void munmap (mapid_t mapping);
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
    case 13:
      {
        int fd = *(int *)(f->esp + 16);
        void *addr = *(void **)(f->esp + 20);  
        f->eax = mmap (fd, addr);
        break;
      }
    case 14:
      {
        mapid_t mapping = *(int *)(f->esp + 4);
        munmap (mapping);
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
  
  /* Close whole mmaped file. */ 
  struct list_elem *e;
  for (e = list_begin (&t->mmap); e != list_end (&t->mmap); e = list_next (e))
    {
      struct mmap_entry *mentry = list_entry (e, struct mmap_entry, mmap_elem);
      munmap (mentry->mapping);
      if (t->remain_cnt <= 0)
        break;
    }
   
  

  /* Close the whole */ 

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

  /* Enroll the file name who is removed. */
  int i;
  for (i = 0; i < 5; i++)
    {
      if (!removed_list[i].is_filled)
        break;
    }
  removed_list[i].is_filled = true;
  memmove (removed_list[i].name, file, 16);
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
    
//    cfl[cfl_idx].is_filled = false;
//    cfl[cfl_idx].is_opened = false;
//    cfl[cfl_idx].file_ptr = NULL;
//    memmove (cfl[cfl_idx].file_name, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);


    file_close (file);
    remove_cfl_fd (fd, file);
    cfl[cfl_idx].is_opened = false;
  }
  lock_release (&close_lock);
}

mapid_t
mmap (int fd, void *addr)
{
  struct thread *t = thread_current ();
  if (addr == (void *) 0xbffff000)
    return -1;
  if (!check_address (addr))
    return -1;
  if (fd == 0 || fd == 1 || free_cnt <= 0 || (unsigned) addr & PGMASK)
    return -1;
  int fmt_idx = search_fmt_idx (fd);
  
  /* If the file whose fd num is FD don't exist, fail. */
  if (fmt_idx == -1)
    return -1;

  int cfl_idx = fmt[fmt_idx].cfl_idx;
  /* If the file is already closed, panic. */
  if (!cfl[cfl_idx].is_opened)
    PANIC ("Trying to map closed file is inhibited!\n");
  struct file *mfile = file_reopen (cfl[cfl_idx].file_ptr);

  /* If the opened file size is 0, fail. */
  int remain_file_length = file_length (mfile);
  if (!remain_file_length)
    {
      file_close (mfile);
      return -1;
    }

  /* Check process virtual address overlap. */
  struct hash_iterator i;
  hash_first (&i, &t->vmhash);
  int page_num = remain_file_length / PGSIZE + remain_file_length % PGSIZE > 0 ? 1 : 0;
  void *low = addr;
  void *high = (void *)((uint32_t)addr + page_num*PGSIZE);
  while (hash_next (&i))
    {
      struct vpage *vpage = hash_entry (hash_cur (&i), struct vpage, h_elem);
      if (vpage->vaddr >= low && vpage->vaddr <= high)
        {
          file_close (mfile);
          return -1;
        }
    }
      
  map_cnt++;

  /* Make mmap_entry. */
  struct mmap_entry *mentry = malloc (sizeof *mentry);
  mentry->file = mfile;
  memmove (mentry->name, cfl[cfl_idx].file_name, 16);
  mentry->mapping = map_cnt + 1;
  mentry->remain_cnt = 0;
  list_init (&mentry->mmap_list);
  list_push_front (&t->mmap, &mentry->mmap_elem);
  t->remain_cnt++;
  

  int page_cnt = 0;
  while (remain_file_length > 0)
    {
      struct vpage *vpage = malloc (sizeof *vpage);
      vpage->type = MMAP;
      memmove (vpage->name, cfl[cfl_idx].file_name, 16);
//      vpage->file = mfile;
      vpage->is_load = false;
      /* True  아닐 수도 있음. */
      vpage->writable = true;
      vpage->vaddr = (void *) ((int) addr + (page_cnt * PGSIZE)); 
      vpage->paddr = NULL;
      vpage->page_read_bytes = remain_file_length > PGSIZE ? PGSIZE : remain_file_length;
      vpage->page_zero_bytes = vpage->page_read_bytes == PGSIZE ? 0 : PGSIZE - remain_file_length;
      vpage->off = page_cnt * PGSIZE; 
      vpage->file = mfile;
      
      list_push_front (&mentry->mmap_list, &vpage->m_elem);    
      mentry->remain_cnt++;
      hash_insert (&t->vmhash, &vpage->h_elem);

      /* Advance. */
      remain_file_length -= PGSIZE; 
      page_cnt++;
    }
  return map_cnt + 1;
}

void
munmap (mapid_t mapping)
{
  struct thread *t = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&t->mmap); e != list_end (&t->mmap); e = list_next (e))
    {
      struct mmap_entry *mentry = list_entry (e, struct mmap_entry, mmap_elem);
      if (mentry->mapping == mapping)
        {
          struct list_elem *e_;
          for (e_ = list_begin (&mentry->mmap_list); e_ != list_end (&mentry->mmap_list); e_ = list_next (e_))
            {
              struct vpage *vpage = list_entry (e_, struct vpage, m_elem);
              file_write_at (mentry->file, vpage->paddr, vpage->page_read_bytes, vpage->off);
              e_ = list_remove (&vpage->m_elem);
              e_ = list_prev (e_);
              mentry->remain_cnt--;
              hash_delete (&t->vmhash, &vpage->h_elem);
              free (vpage);
              if (mentry->remain_cnt <= 0)
                break;
            }
          list_remove (&mentry->mmap_elem);
          t->remain_cnt--;
          
          int i;
          bool is_removed = false;
          for (i = 0; i < 5; ++i)
            {
              if (!strcmp (mentry->name, removed_list[i].name))
                {
                  is_removed = true;
                  break;
                }
            }
          if (!is_removed)
            file_close (mentry->file);
          free (mentry);
        } 
      if (t->remain_cnt <= 0)
        break;
    }
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

