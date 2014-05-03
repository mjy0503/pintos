#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "vm/page.h"

struct frame_entry{
  void *frame;
  struct page_entry *page;
  struct list_elem elem;
};

void frame_init(void);
void *frame_alloc(enum palloc_flags flags, struct page_entry *p);
void frame_free(void *frame);

#endif
