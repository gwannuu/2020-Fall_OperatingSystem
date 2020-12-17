#include "vm/frame.h"
#include <debug.h>
#include "threads/malloc.h"

void
frame_fill (struct vpage *vpage)
{
  struct page *page = malloc (sizeof *page);
  page->vpage = vpage;
  struct thread *t = thread_current ();
  page->thread = t;
  list_push_front (&p_mem, &page->list_elem);
  free_cnt--;
}

void
frame_remove (struct vpage *vpage)
{
  struct list_elem *e;
  for (e = list_begin (&p_mem); e != list_end (&p_mem); e = list_next (e))
    {
      struct page *page = list_entry (e, struct page, list_elem);
      if (page->vpage == vpage)
        {
          list_remove (e);
          free (page);
          free_cnt++;
          return;
        }
    }
}

