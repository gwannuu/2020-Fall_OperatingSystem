#include "vm/page.h"
#include "threads/malloc.h"

unsigned
vpage_hash (const struct hash_elem *v_, void *aux)
{
  const struct vpage *v = hash_entry (v_, struct vpage, h_elem);
  return hash_bytes (&v->vaddr, sizeof v->vaddr);
}

bool
vpage_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux)
{
  const struct vpage *a = hash_entry (a_, struct vpage, h_elem);
  const struct vpage *b = hash_entry (b_, struct vpage, h_elem);
  return a->vaddr < b->vaddr;
}

void
vpage_action (struct hash_elem *e, void *aux)
{
  struct vpage *vpage = hash_entry (e, struct vpage, h_elem);
  free (vpage);
}

/*
struct vpage *
vpage_lookup (struct hash *vmhash, const void *addr)
{
  struct vpage v;
  struct hash_elem *e;

  v.vaddr = addr;
  e = hash_find (vmhash, v.h_elem);
  return e != NULL ? hash_entry (e, struct page, h_elem) : NULL;
}
*/
