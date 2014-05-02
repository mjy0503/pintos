#include "vm/page.h"

struct list frame_table;

struct lock frame_lock;

struct frame_entry{
  void *frame;
  struct page_entry *page;
  struct list_elem elem;
};

void init_frame();
void *alloc_frame(enum palloc_flags flags);
void free_frame(void *frame);

