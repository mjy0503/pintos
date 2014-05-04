#include "vm/swap.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>

struct bitmap *swap_bitmap;
struct disk *swap_disk;
struct lock swap_lock;

#define SECTOR_NUM (PGSIZE/DISK_SECTOR_SIZE)

void swap_init(void){
  swap_disk = disk_get(1,1);
  swap_bitmap = bitmap_create(disk_size(swap_disk));
  lock_init(&swap_lock);
}

void swap_in(size_t swap_index, void *addr){ // disk -> memory
  int i;
  lock_acquire(&swap_lock);
  for(i=0;i<SECTOR_NUM;i++){
    disk_read(swap_disk, swap_index+i, addr + i * DISK_SECTOR_SIZE);
  }
  bitmap_set_multiple(swap_bitmap, swap_index, SECTOR_NUM, 0);
  lock_release(&swap_lock);
}

void swap_free(size_t swap_index){
  lock_acquire(&swap_lock);
  bitmap_set_multiple(swap_bitmap, swap_index, SECTOR_NUM, 0);
  lock_release(&swap_lock);
}

size_t swap_out(void *addr){ // memory -> disk
  lock_acquire(&swap_lock);
  size_t swap_index = bitmap_scan_and_flip(swap_bitmap, 0, SECTOR_NUM, 0);
  int i;
  if(swap_index == BITMAP_ERROR)
    PANIC("no space in disk");
  for(i=0;i<SECTOR_NUM;i++){
    disk_write(swap_disk, swap_index+i, addr + i * DISK_SECTOR_SIZE);
  }
  lock_release(&swap_lock);
  return swap_index;
}

