#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

void cache_init(void);
void cache_close(void);
void cache_read(disk_sector_t sector, void *buffer, off_t ofs, off_t size);
void cache_write(disk_sector_t sector, const void *buffer, off_t ofs, off_t size);
void cache_evict(void);

#endif
