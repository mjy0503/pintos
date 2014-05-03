#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>

enum page_stat{
  FRAME,
  SWAP_SLOT
};

struct page_entry{
  /* common */
  void *page;                     /* virtual address */
  enum page_stat status;          /* FILE, SWAP */
  bool writable;                  /* writable page */

  struct hash_elem hash_elem;     /* hash element */
};

bool page_table_init(struct hash *page_table);
void page_table_destroy(struct hash *page_table);
struct page_entry *page_create(struct hash *page_table, void *addr, bool writable);
struct page_entry *page_find(struct hash *page_table, void *addr);
void page_delete(struct hash *page_table, struct page_entry *p);
bool page_load(struct hash *page_table, void *addr, uint32_t *pagedir);

#endif
