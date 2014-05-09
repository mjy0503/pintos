#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
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
  struct page_entry *p = hash_entry(hash_elem, struct page_entry, hash_elem);
  if(p->status == SWAP_SLOT)
    swap_free(p->swap_index);
  free(p);
}

bool page_table_init(struct hash *page_table){
  return hash_init(page_table, page_hash_func, page_less_func, NULL);
}

void page_table_destroy(struct hash *page_table){
  hash_destroy(page_table, page_action_func);
}

struct page_entry *page_create(struct hash *page_table, void *addr, bool writable){
  struct page_entry *p = malloc(sizeof(struct page_entry));
  if(p == NULL)
    return false;

  p->page = pg_round_down(addr);
  p->status = FRAME;
  p->writable = writable;
  p->file = NULL;

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
      break;
    case SWAP_SLOT:
    {
      uint8_t *kpage = frame_alloc(PAL_USER, p);
      if(kpage == NULL)
        return false;

      swap_in(p->swap_index, kpage);
      if(pagedir_get_page(pagedir, p->page)!=NULL || !pagedir_set_page(pagedir, p->page, kpage, p->writable)){
        frame_free(kpage);
        return false;
      }

      p->status = FRAME;
      p->pin = false;
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

      file_seek(p->file, p->offset);
      if (file_read (p->file, kpage, p->read_bytes) != (int) p->read_bytes)
        {
          frame_free (kpage);
          return false; 
        }
      memset (kpage + p->read_bytes, 0, p->zero_bytes);

      if(pagedir_get_page(pagedir, p->page)!=NULL || !pagedir_set_page(pagedir, p->page, kpage, p->writable)){
        frame_free(kpage);
        return false;
      }
      p->status = FRAME;
      p->pin = false;
      break;
    }
    default:
      PANIC("WHAT STATUS OF LOAD?");
  }
  return true;
}

bool stack_growth(struct hash *page_table, void *addr, uint32_t *pagedir){
  struct page_entry *p = page_create(page_table, addr, true);
  if(p == NULL) return false;
  
  uint8_t *kpage = frame_alloc(PAL_USER | PAL_ZERO, p);
  if(kpage == NULL){
    page_delete(page_table, p);
    return false;
  }
  if(pagedir_get_page(pagedir, p->page)!=NULL || !pagedir_set_page(pagedir, p->page, kpage, p->writable)){
    page_delete(page_table, p);
    frame_free(kpage);
    return false;
  }
  return true;
}
