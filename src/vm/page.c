#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

unsigned page_hash_func(const struct hash_elem *p, void *aux UNUSED){
  return hash_int((int)(hash_entry(p, struct page_entry, hash_elem)->page));
}

bool page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  return hash_entry(a, struct page_entry, hash_elem)->page < hash_entry(b, struct page_entry, hash_elem)->page;
}

void page_action_func(struct hash_elem *hash_elem, void *aux UNUSED){
  free(hash_entry(hash_elem, struct page_entry, hash_elem));
}

bool page_table_init(struct hash *page_table){
  return hash_init(page_table, page_hash_func, page_less_func, NULL);
}

void page_table_destroy(struct hash *page_table){
  hash_destroy(page_table, page_action_func);
}

struct page_entry *page_create(struct hash *page_table, void *addr, bool writable, bool status){ // 0 = FRAME, 1 = ZERO
  struct page_entry *p = malloc(sizeof(struct page_entry));
  if(p == NULL)
    return false;

  p->page = pg_round_down(addr);
  if(status)
    p->status = ALL_ZERO;
  else
    p->status = FRAME;
  p->writable = writable;

  if(hash_insert(page_table, &p->hash_elem) != NULL)
    free(p);
  return p;
}

struct page_entry *page_create_file(struct hash *page_table, void *addr, bool writable, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes){
  struct page_entry *p = malloc(sizeof(struct page_entry));
  if(p == NULL)
    return false;

  p->page = pg_round_down(addr);
  p->status = FILE_SYS;
  p->writable = writable;
  p->file = file;
  p->offset = offset;
  p->read_bytes = read_bytes;
  p->zero_bytes = zero_bytes;

  if(hash_insert(page_table, &p->hash_elem) != NULL)
    free(p);
  return p;
}

struct page_entry *page_find(struct hash *page_table, void *addr){
  struct page_entry p;
  struct hash_elem *e;
  p.page = (void *)pg_round_down(addr);
  if((e = hash_find(page_table, &p.hash_elem)) == NULL)
    return NULL;
  return hash_entry(e, struct page_entry, hash_elem);
}

void page_delete(struct hash *page_table, struct page_entry *p){
  hash_delete(page_table, &p->hash_elem);
  free(p);
}

bool page_load(struct hash *page_table, void *addr, uint32_t *pagedir){
  struct page_entry *p = page_find(page_table, addr);
  if(p == NULL)
    return false;
  switch(p->status){
    case FRAME:
      PANIC("Already in frame!");
      break;
    case SWAP_SLOT:
    {
      break;
    }
    case FILE_SYS:
    {
      uint8_t *kpage;
      if(p->zero_bytes == PGSIZE)
        kpage = frame_alloc(PAL_USER | PAL_ZERO, p);
      else
        kpage = frame_alloc(PAL_USER, p);
      if (kpage == NULL)
        return false;

      lock_acquire(&file_lock);
      file_seek(p->file, p->offset);
      if (file_read (p->file, kpage, p->read_bytes) != (int) p->read_bytes)
        {
          lock_release(&file_lock);
          frame_free (kpage);
          return false; 
        }
      lock_release(&file_lock);
      memset (kpage + p->read_bytes, 0, p->zero_bytes);

      p->status = FRAME;
      if(!pagedir_set_page(pagedir, p->page, kpage, p->writable))
        return false;
      break;
    }
    case ALL_ZERO:
    {
      break;
    }
    default:
      PANIC("WHAT STATUS OF LOAD?");
  }
  return true;
}

