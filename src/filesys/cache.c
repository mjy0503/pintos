#include "filesys/cache.h"
#include <list.h>
#include <string.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"

#define CACHE_SIZE 64
struct cache_entry
{
  void *data;
  disk_sector_t sector;
  bool dirty;
  struct list_elem elem;
};
//TODO read_ahead, write_behind timer?
static int cache_count;

struct lock cache_lock;
struct list cache_list;

void cache_init(){
  lock_init(&cache_lock);
  list_init(&cache_list);
}

void cache_close(){
  lock_acquire(&cache_lock);
  struct list_elem *e;
  struct cache_entry *c;
  while(!list_empty(&cache_list)){
    e = list_pop_front(&cache_list);
    c = list_entry(e, struct cache_entry, elem);
    if(c->dirty)
      disk_write(filesys_disk, c->sector, c->data);
    free(c->data);
    free(c);
  }
  cache_count = 0;
  lock_release(&cache_lock);
}

void cache_read(disk_sector_t sector, void *buffer, off_t ofs, off_t size){
  lock_acquire(&cache_lock);
  struct cache_entry *c = NULL;
  struct list_elem *e;
  for(e = list_begin(&cache_list); e != list_end(&cache_list); e = list_next(e)){
    c = list_entry(e, struct cache_entry, elem);
    if(c->sector == sector)
      break;
  }
  if(e == list_end(&cache_list)){
    if(cache_count == CACHE_SIZE)
      cache_evict();
    cache_count++;
    c = malloc(sizeof(struct cache_entry));
    c->data = malloc(DISK_SECTOR_SIZE);
    c->dirty = 0;
    c->sector = sector;
    list_push_back(&cache_list, &c->elem);
    disk_read(filesys_disk, c->sector, c->data);
  }

  memcpy(buffer, c->data+ofs, size);
  lock_release(&cache_lock);
}

void cache_write(disk_sector_t sector, const void *buffer, off_t ofs, off_t size){
  lock_acquire(&cache_lock);
  struct cache_entry *c = NULL;
  struct list_elem *e;
  for(e = list_begin(&cache_list); e != list_end(&cache_list); e = list_next(e)){
    c = list_entry(e, struct cache_entry, elem);
    if(c->sector == sector)
      break;
  }
  if(e == list_end(&cache_list)){
    if(cache_count == CACHE_SIZE)
      cache_evict();
    cache_count++;
    c = malloc(sizeof(struct cache_entry));
    c->data = malloc(DISK_SECTOR_SIZE);
    c->sector = sector;
    list_push_back(&cache_list, &c->elem);
    if(ofs>0 || size<DISK_SECTOR_SIZE)
      disk_read(filesys_disk, c->sector, c->data);
  }
  c->dirty = 1;
  memcpy(c->data+ofs, buffer, size);
  lock_release(&cache_lock);
}

void cache_evict(){
  cache_count--;
  struct cache_entry *c;
  c = list_entry(list_pop_front(&cache_list), struct cache_entry, elem);
  if(c->dirty)
    disk_write(filesys_disk, c->sector, c->data);
  free(c->data);
  free(c);
}
