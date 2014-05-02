#include "vm/frame.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include <list.h>

struct list frame_table;
struct lock frame_lock;

void init_frame(){
  list_init(&frame_table);
  lock_init(&frame_lock);
}

void *alloc_frame(enum palloc_flags flags){
  ASSERT((flags & PAL_USER)!=0);
  void *frame = palloc_get_page(flags);
  struct frame_entry *f = malloc(sizeof(struct frame_entry));
  if(!frame){ // frame_evict
    PANIC("evict!");
  }
  f->frame = frame;
  f->page = NULL;
  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &f->elem);
  lock_release(&frame_lock);
  return frame;
}

void free_frame(void *frame){
  struct list_elem *e;
  lock_acquire(&frame_lock);
  for(e = list_begin(&frame_table);e != list_end(&frame_table);e = list_next(e)){
    if(list_entry(e, struct frame_entry, elem)->frame == frame){
      list_remove(e);
      free(list_entry(e, struct frame_entry, elem));
      break;
    }
  }
  lock_release(&frame_lock);
  palloc_free_page(frame);
}

void evict_frame(){
}
