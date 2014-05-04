#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include <list.h>
#include <debug.h>

struct list frame_table;
struct lock frame_lock;

void frame_init(){
  list_init(&frame_table);
  lock_init(&frame_lock);
}

void *frame_alloc(enum palloc_flags flags, struct page_entry *p){
  ASSERT((flags & PAL_USER)!=0);
  void *frame = palloc_get_page(flags);
  struct frame_entry *f = malloc(sizeof(struct frame_entry));
  while(frame == NULL){
    if(!frame_evict(flags))
      return NULL;
    frame = palloc_get_page(flags);
  }
  f->frame = frame;
  f->page = p;
  f->thread = thread_current();
  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &f->elem);
  lock_release(&frame_lock);
  return frame;
}

void frame_free(void *frame){
  struct list_elem *e;
  lock_acquire(&frame_lock);
  for(e = list_begin(&frame_table);e != list_end(&frame_table);e = list_next(e)){
    if(list_entry(e, struct frame_entry, elem)->frame == frame){
      list_remove(e);
      free(list_entry(e, struct frame_entry, elem));
      break;
    }
  }
  palloc_free_page(frame);
  lock_release(&frame_lock);
}

bool frame_evict(enum palloc_flags flags){
  struct list_elem *e;
  struct frame_entry *f;
  lock_acquire(&frame_lock);
  for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
    f = list_entry(e, struct frame_entry, elem);
    pagedir_clear_page(f->thread->pagedir, f->page->page);
    if(f->page->file == NULL || f->page->writable){
      f->page->status = SWAP_SLOT;
      f->page->swap_index = swap_out(f->frame);
    }
    else
      f->page->status = FILE_SYS;
    list_remove(e);
    palloc_free_page(f->frame);
    free(f);
    break;
  }
  lock_release(&frame_lock);
  return true;
}
