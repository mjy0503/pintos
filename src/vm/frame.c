#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
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
  lock_acquire(&frame_lock);
  void *frame = palloc_get_page(flags);
  struct frame_entry *f = malloc(sizeof(struct frame_entry));
  while(frame == NULL){
    if(!frame_evict(flags)){
      lock_release(&frame_lock);
      return NULL;
    }
    frame = palloc_get_page(flags);
  }
  f->frame = frame;
  f->page = p;
  f->page->pin = true;
  f->thread = thread_current();
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
      palloc_free_page(frame);
      break;
    }
  }
  lock_release(&frame_lock);
}

bool frame_evict(enum palloc_flags flags){
  struct list_elem *e;
  struct frame_entry *f;
  for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
    f = list_entry(e, struct frame_entry, elem);
    if(!f->page->pin){
      lock_acquire(&f->thread->pagedir_lock);
      if(f->thread->pagedir == NULL){
        lock_release(&f->thread->pagedir_lock);
        continue;
      }
      pagedir_clear_page(f->thread->pagedir, f->page->page);
      if(f->page->status == FRAME_MMAP){
        struct page_entry *p = f->page;
        p->pin = true;
        lock_acquire(&file_lock);
        if(pagedir_is_dirty(f->thread->pagedir, p->page))
          file_write_at(p->file, p->page, p->read_bytes, p->offset);
        lock_release(&file_lock);
        p->status = MMAP;
        p->pin = false;
      }
      else if(f->page->file == NULL || f->page->writable){
        f->page->status = SWAP_SLOT;
        f->page->swap_index = swap_out(f->frame);
      }
      else
        f->page->status = FILE_SYS;
      lock_release(&f->thread->pagedir_lock);
      list_remove(e);
      palloc_free_page(f->frame);
      free(f);
      break;
    }
  }
  return true;
}
