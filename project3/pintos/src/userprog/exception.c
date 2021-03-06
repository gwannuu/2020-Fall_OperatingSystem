#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/swap.h"
/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static bool install_page (void *upage, void *kpage, bool writable);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
//      printf ("%s: dying due to interrupt %#04x (%s).\n",
//              thread_name (), f->vec_no, intr_name (f->vec_no));
//      intr_dump_frame (f);
//      thread_exit (); 
      exit (-1);
    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
//      intr_dump_frame (f);
//      PANIC ("Kernel bug - unexpected interrupt in kernel");
//      printf ("SEL_KCSEG: \t");
      exit (-1);

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
//  bool not_present;  /* True: not-present page, false: writing r/o page. */
//  bool write;        /* True: access was write, false: access was read. */
//  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */


//  printf ("fault addr : %p\n", fault_addr);
//  printf ("f->esp addr : %p\n", fault_addr);
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  struct thread *t = thread_current ();

//  if (!strcmp(t->name, "pt-grow-bad"))
//    exit (-1);
  struct hash_iterator i;
  struct vpage *vpage;
  struct file *file = NULL;
  bool success = false;
  int smt_idx = -1;
  unsigned fault_addr_pgmask = (unsigned) fault_addr & ~PGMASK;

//  printf("fault : %#x\n", fault_addr);
//  printf("stack : %#x\n", t->stack_limit);
//  printf("limit : %#x\n", t->page_limit);
  /* Page fault handler for stack growth. */
//  printf("      %lu      \n", t->stack_limit);
/*
  if (fault_addr < t->page_limit && fault_addr > t->stack_limit)
    {
      exit (-1);
    }
*/
/*
  if ((unsigned)fault_addr < t->stack_limit ||
      f->esp - 4 != fault_addr ||
      f->esp - 32 != fault_addr)
    {
        printf ("f->esp : %p fault_addr %p\n",f->esp, fault_addr);
        printf ("A\n");
        exit(-1);
    }
*/

//  printf("f->esp : %p\nfault_ad %p\n",f->esp, fault_addr);
  if (fault_addr_pgmask >= t->stack_limit
        && fault_addr_pgmask < t->stack_limit + 0x800000)
    {
      if (fault_addr + 0x1000 <= f->esp && f->esp <= fault_addr + 0x40000)
        {
//          printf ("f->esp : %p\nfault_ad %p\n",f->esp, fault_addr);
          exit(-1);
        }
      uint8_t *kpage = palloc_get_page (PAL_USER);
      install_page((void *) fault_addr_pgmask, kpage, true);
      return;
    }
/*
 
  if (fault_addr < t->page_limit && fault_addr > t->stack_limit)
    {
      printf("fault : %#x\n", fault_addr);
      printf("stack : %#x\n", t->stack_limit);
      printf("limit : %#x\n", t->page_limit);
      exit (-1);
    }
*/

  enum type type;
  /* Search whole vpage. */
  hash_first (&i, &t->vmhash);
  while (hash_next (&i))
    {
      vpage = hash_entry (hash_cur (&i), struct vpage, h_elem);        
      if ((unsigned) vpage->vaddr == ((unsigned) fault_addr & ~PGMASK))
        {
//          ASSERT (!vpage->is_load);
          if (free_cnt <= 0)
            {
              /* We should evict one frame. */
            //  size_t slot_idx = swap_slot_scan (false);
           //   int smt_idx = swap_smt_empty ();
                            
            }
          uint8_t *kpage = palloc_get_page (PAL_USER);
          ASSERT (kpage != NULL);
          vpage->paddr = (void *) kpage;

          /* Set the frame table. */
          struct page *page = malloc (sizeof *page);
          frame_fill (vpage);

          /* Allocate file pointer FILE. */
          if (vpage->type == NORMAL)
            {
              discontinue_until_acquire_lock (&filesys_lock);
              file = filesys_open (vpage->name);
              lock_release (&filesys_lock);
              type = NORMAL;
            }
          else if (vpage->type == MMAP)
            {
              file = vpage->file;
              type = MMAP;
            }

          if (file == NULL)
            exit (-1);
          file_seek (file, vpage->off);
          if (file_read (file, kpage, vpage->page_read_bytes) != (int) 
vpage->page_read_bytes)
            {
              palloc_free_page (kpage);
//              printf ("Pull a new page fail!");
              exit(-1);
            }
          memset (kpage + vpage->page_read_bytes, 0, vpage->page_zero_bytes);
          if (!install_page (vpage->vaddr, kpage, vpage->writable))    
            {
              palloc_free_page (kpage);
//              printf ("Pull a new page fail!");
              exit(-1);
            }
          vpage->is_load = true;
          success = true;
          break;
        }
    }
  if (type == NORMAL)
    file_close (file);
  if (!success)
    exit (-1);
  return;
}

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL    
          && pagedir_set_page (t->pagedir, upage, kpage, writable));   
}
