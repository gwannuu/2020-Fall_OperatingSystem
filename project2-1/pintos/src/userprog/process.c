#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#include "filesys/file.h"
#include "filesys/directory.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp, char *args);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  if (!is_start)
    {
      lock_init (&exit_lock);
      lock_init (&exec_lock);
      lock_init (&wait_lock);
      lock_init (&create_lock);
      lock_init (&remove_lock);
      lock_init (&open_lock);
      lock_init (&filesize_lock);
      lock_init (&read_lock);
      lock_init (&write_lock);
      lock_init (&seek_lock);
      lock_init (&tell_lock);
      lock_init (&close_lock);
      lock_init (&filesys_lock);
      is_start = true;
    }
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Copy file name FILE_NAME. */
  int size = strlen (file_name);
  char fn_copy_[size + 1];
  char *args;
  strlcpy (fn_copy_, file_name, size + 1);
  char *file_name_ = strtok_r (fn_copy_, " ", &args);
  int charidx = 0;
  while (file_name_[charidx] != ' ' && file_name_[charidx] != '\0') 
    charidx++;
  
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  
  if (!dir_lookup (dir, file_name_, &inode))
    return -1;
   

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
 
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  
  int idx = search_empty ();
  pmt[idx].is_filled = true;
  pmt[idx].thread_id = tid;
  pmt[idx].parent_thread = thread_current ();
  memmove (pmt[idx].name, file_name_, charidx+1 > 16 ? 16 : charidx+1);//  int par_idx = search_idx (thread_current ()->tid);
//  insert_wcl (par_idx, tid);  

  proc_cnt++;
 
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  int length = strlen (file_name_);
  char cl[length + 1];
  memcpy (cl, file_name_, length + 1); 
  
  char *file_args; 
  char *file_name = strtok_r (file_name_, " ", &file_args);
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp, cl);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  
  struct thread *t = thread_current ();    

  /* Find the child index of process management table. */
  int chd_idx = search_idx (child_tid);
  if (chd_idx == -1)
    return -1;

  /* If child_tid process is grandchild, return -1. (Doesn't match) */
  else if (pmt[chd_idx].parent_thread != t)
    return -1;

  /* Child is already dead by kernel. */
  if (pmt[chd_idx].is_dead == true)
    return pmt[chd_idx].ret_val;
  
  /* Child is not dead. */
  if (child_tid == 3)
  {
    enum intr_level old_level = intr_get_level ();
    intr_set_level (INTR_OFF);
    thread_block ();
    intr_set_level (old_level);
  } 
  else
  { 
    int par_idx = search_idx (t->tid);
    /* Check whether parent is waiting child. */
    if (!check_wcl (par_idx, child_tid))
    {
      pmt[par_idx].waiting_child_cnt++;
      insert_wcl (par_idx, child_tid);
      enum intr_level old_level = intr_get_level ();
      intr_set_level (INTR_OFF);
      thread_block ();
      intr_set_level (old_level);
//      remove_wcl (par_idx, child_tid);
    }
     /* Wait twice. */
    else
      return -1;
  }
  return pmt[chd_idx].ret_val;
}

/* check whether pid of child is in the parent's waiting child list. */
bool
check_wcl (int idx, tid_t child_tid)
{
  int i = 0;
  while (pmt[idx].waiting_child_list[i] != child_tid)
    {
      i++;
      if (i >= WCL_SIZE)
        return false;
    }
  return true;
}

/* Insert the pid of child in parent's waiting child list. */
void
insert_wcl (int idx, tid_t child_tid)
{
  int empty_idx = 0;
  while (pmt[idx].waiting_child_list[empty_idx] != 0)
    {
      empty_idx++;
      ASSERT (empty_idx < WCL_SIZE);
    }
  pmt[idx].waiting_child_list[empty_idx] = child_tid;
}

/* Remove the pid of child in parent's waiting child list. */
void
remove_wcl (int idx, tid_t child_tid)
{
  int i = 0;
  while (pmt[idx].waiting_child_list[i] != child_tid)
    {
      i++;
      ASSERT (i < WCL_SIZE);
    }
  pmt[idx].waiting_child_list[i] = 0;
}

/* Insert file descriptor set for a file in curernt file list. */
void
insert_cfl_fd (int fd, struct file *file)
{
  int idx = 0;
  while (cfl[idx].file_ptr != file)
    {
      idx++;
      ASSERT (idx < CFL_SIZE);
    }
  int fd_idx = 0;
  while (cfl[idx].fd[fd_idx] != 0)
    {
      fd_idx++;
      ASSERT (fd_idx < FD_SIZE);
    }
  cfl[idx].fd[fd_idx] = fd;
}

/* Remove file descriptor set for a file in current file list. */
void
remove_cfl_fd (int fd, struct file *file)
{
  
  int idx = 0;
  while (cfl[idx].file_ptr != file)
    {
      idx++;
      ASSERT (idx < CFL_SIZE);
    }
  int fd_idx = 0;
  while (cfl[idx].fd[fd_idx] != fd)
    {
      fd_idx++;
      ASSERT (fd_idx < FD_SIZE);
    }
  cfl[idx].fd[fd_idx] = 0;
}
/* Find the idx in Process Management Table for TID thread. */
int
search_idx (tid_t tid)
{
  int i = 0;
  while (pmt[i].thread_id != tid)
  {
    i++;
    if (i >= PMT_SIZE)
      return -1;
  }
  return i;
}
  
/* Find the foremost blank idx in Process Management Table. */
int
search_empty (void)
{
  int i = 0;
  while (pmt[i].is_filled)
  {
    i++;
    ASSERT (i <= PMT_SIZE);
  }
  return i;
}

/* Find the File Descripter Set index in pmt[idx]. */
int
search_fds_idx (int idx, int fd)
{
  int i = 0;
  while (pmt[idx].fds[i].fd != fd)
    {
      i++;
      if (i >= FD_SIZE)
        return -1;
    }
  return i;
}

/* Find the File Mapping index which is empty. */
int
search_fds_empty (int idx)
{
  int i = 0;
  while (pmt[idx].fds[i].fd != 0)
    {
      i++;
      ASSERT (i < FMT_SIZE);
    }
  return i;
}

/* Find the index of file name FN in current file list. */
int
search_cfl_idx_same_fn_filename (const char *fn)
{
  int i = 0;
  while (strcmp(cfl[i].file_name, fn))
    {
      i++;
      if (i >= CFL_SIZE)
        return -1;
    }
  return i;
}

/* Find the index of current file list which is empty. */
int
search_cfl_empty (void)
{
  int i = 0;
  while (cfl[i].is_filled)
    {
      i++;
      ASSERT (i < CFL_SIZE);
    }
  return i;
}

/* Find the index of file mapping table which is same with fd. */
int
search_fmt_idx (int fd)
{
  if (fd == 0)
    return -1;
  int i = 0;
  while (fmt[i].fd != fd)
  {
    i++;
    if (i >= FMT_SIZE)
      return -1;
  }
  return i;
}

/* Find the index of file mapping table which is empty. */
int
search_fmt_empty (void)
{
  int i = 0;
  while (fmt[i].fd != 0)
  {
    i++;
    ASSERT (i <FMT_SIZE)
  }
  return i;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  proc_cnt--;
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *cl);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp, char *cl) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;

  /* Open executable file. */
  discontinue_until_acquire_lock (&filesys_lock);
  file = filesys_open (file_name);
  lock_release (&filesys_lock);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, cl))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *cl) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE * proc_cnt, kpage, true);
      if (success){
        *esp = PHYS_BASE - PGSIZE * (proc_cnt - 1);
//        *esp = PHYS_BASE - PGSIZE;

        /* Initialize the variables. */
        int ptr = strlen(cl) - 1;
        int count = 1;
        int size = 0;
        int args_cnt = 0;
        char *cl_addr[30];
        int j;
 
        /* Set the memory to '0'. */
        for (j = 0; j < 30; ++j)
          cl_addr[j] = 0;

        /* Push arguments. */
        while (ptr > 0) 
        {
          ptr--;
          count++;
          if (cl[ptr] == 0x20)
          {
            ptr++;
            count--; 
            memmove (*(char **)esp - count - 1, &cl[ptr], count); 
            memset (*(char **)esp - 1, '\0', 1);
            *(char **)esp -= count + 1;

            cl_addr[args_cnt] = *esp;
            size += count + 1;

            /* Set for cl[ptr] to point the char which is not 0x20. */
            if (cl[ptr-2] == 0x20)
            {
              ptr -= 1;
              while (cl[ptr] == 0x20)
                ptr -= 1;
            }
            else
              ptr -= 2;
            count = 1;
            args_cnt++;
          }
          else if (ptr == 0)
          {
            memmove (*(char **)esp - count - 1, &cl[ptr], count);
            memset (*(char **)esp - 1, '\0', 1);
            *(char **)esp -= count + 1;
            cl_addr[args_cnt] = *esp;
            size += count + 1;
            args_cnt ++;
            break;
          }
        } 

        /* Word-Align. */
        int remainder = size % 4;
        if (remainder != 0)
        {
          int i;
          for (i = 0; i < remainder; i++)
          {
            memset (*(char **)esp - 1, '\0', 1);
            *(char **)esp -= 1;
          }
        }

        /* argv[n]. */   
        memset (*(char **)esp - 4, '\0', 4);
        *(char **)esp -= 4;

        /* argv[n-1] ~ argv[0]. */
        int i;
        for (i = 0; i < args_cnt; i++) 
        {
          memmove (*(char **)esp - 4, &cl_addr[i], 4);
          *(char **)esp -= 4;
        } 

        /* argv, argc. */
        memmove (*(char **)esp - 4, (char **)esp, 4);
        *(char **)esp -= 4;
        *(int **)esp -= 1;
        **(int **)esp = args_cnt;

        /* return address. */
        *(int **)esp -= 1;
        **(int **)esp = 0; 
    
      }
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void
discontinue_until_acquire_lock (struct lock *lock)
{
  while (true)
    {
      if (lock_held_by_current_thread (lock))
        break;
      else
        if (!lock_try_acquire (lock))
          continue;
    }
}
