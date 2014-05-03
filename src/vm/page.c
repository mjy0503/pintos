#include "vm/page.h"
#include "filesys/file.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <stdbool.h>

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

struct page_entry *page_create(struct hash *page_table, void *addr, bool writable){
  struct page_entry *p = malloc(sizeof(struct page_entry));
  if(p == NULL)
    return false;

  p->page = pg_round_down(addr);
  p->status = FRAME;
  p->writable = writable;

  if(hash_insert(page_table, &p->hash_elem) != NULL)
    free(p);
  return p;
}

void page_delete(struct hash *page_table, struct page_entry *p){
  hash_delete(page_table, &p->hash_elem);
  free(p);
}

struct page_entry *page_find(struct hash *page_table, void *addr){
  struct page_entry p;
  struct hash_elem *e;
  p.page = (void *)pg_round_down(addr);
  if((e = hash_find(page_table, &p.hash_elem)) == NULL)
    return NULL;
  return hash_entry(e, struct page_entry, hash_elem);
}

bool page_load(struct hash *page_table, void *addr, uint32_t *pagedir){
  struct page_entry *p = page_find(page_table, addr);
  if(p == NULL)
    return false;
  switch(p->status){
    case FRAME:
      break;
    case SWAP_SLOT:
      break;
    default:
      PANIC("WHAT STATUS OF LOAD?");
  }
  return true;
}

