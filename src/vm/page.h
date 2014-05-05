#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "filesys/file.h"

enum page_stat{
  FRAME,
  SWAP_SLOT,
  FILE_SYS,
  ALL_ZERO
};

struct page_entry{
  /* common */
  void *page;                     /* virtual address */
  enum page_stat status;          /* FILE, SWAP */
  bool writable;                  /* writable page */
  bool pin;                       /* pinning */

  /* filesys */
  struct file *file;              /* reading file */
  off_t offset;                   /* file offset */
  uint32_t read_bytes;            /* read bytes from file */
  uint32_t zero_bytes;            /* zero bytes */

  size_t swap_index;              /* start index of swap disk */

  struct hash_elem hash_elem;     /* hash element */
};

bool page_table_init(struct hash *page_table);
void page_table_destroy(struct hash *page_table);
struct page_entry *page_create(struct hash *page_table, void *addr, bool writable);
struct page_entry *page_create_file(struct hash *page_table, void *addr, bool writable, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes);
struct page_entry *page_find(struct hash *page_table, void *addr);
void page_delete(struct hash *page_table, struct page_entry *p);
bool page_load(struct hash *page_table, void *addr, uint32_t *pagedir);
bool stack_growth(struct hash *page_table, void *addr, uint32_t *pagedir);

#endif
