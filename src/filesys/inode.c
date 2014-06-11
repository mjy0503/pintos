#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_INODE 100
#define SINGLE_INDIRECT_INODE 100
#define DOUBLE_INDIRECT_INODE 2


/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t count;                     /* number of inode */
    uint32_t level;                     /* level of inode */
    disk_sector_t inode_index[100];     /* sector number of inode */
    disk_sector_t parent_sector;        /* Sector number of parent (only level 2)*/
    uint32_t is_dir;                    /* check inode is directory */
    uint32_t unused[22];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    pos /= DISK_SECTOR_SIZE;
    struct inode_disk *disk_inode = calloc(1, sizeof(struct inode_disk));
    if(disk_inode == NULL) return -1;
    disk_sector_t sector;
    cache_read(inode->data.inode_index[pos/(DIRECT_INODE*SINGLE_INDIRECT_INODE)], disk_inode, 0, DISK_SECTOR_SIZE);
    pos%=DIRECT_INODE*SINGLE_INDIRECT_INODE;
    sector = disk_inode->inode_index[pos/SINGLE_INDIRECT_INODE];
    cache_read(sector, disk_inode, 0, DISK_SECTOR_SIZE);
    sector = disk_inode->inode_index[pos%SINGLE_INDIRECT_INODE];
    free(disk_inode);
    return sector;
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, int level, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode->level = level;
    disk_inode->is_dir = is_dir;
    if(level == 0){ // direct
      disk_inode->count = sectors;
      if (free_map_allocate (sectors, disk_inode->inode_index)){
        cache_write (sector, disk_inode, 0, DISK_SECTOR_SIZE);
        static char zeros[DISK_SECTOR_SIZE];
        size_t i;

        for (i = 0; i < disk_inode->count; i++) 
          cache_write (disk_inode->inode_index[i], zeros, 0, DISK_SECTOR_SIZE); 
        success = true; 
      } 
    }
    else if(level == 1){ // sigle indirect
      disk_inode->count = DIV_ROUND_UP(sectors, DIRECT_INODE);
      if(free_map_allocate (disk_inode->count, disk_inode->inode_index)){
        size_t size;
        int i;
        for (i = 0; i < disk_inode->count; i++){
          size = sectors - DIRECT_INODE*i;
          if(size > DIRECT_INODE)
            size = DIRECT_INODE;
          if(!inode_create(disk_inode->inode_index[i], size*DISK_SECTOR_SIZE, level-1, is_dir)){
            success = false;
            for(i=i-1;i>=0;i--)
              inode_delete(disk_inode->inode_index[i]);
            break;
          }
        }
        if(i == disk_inode->count){
          cache_write (sector, disk_inode, 0, DISK_SECTOR_SIZE);
          success = true; 
        }
      }
    }
    else{ // double indirect
      disk_inode->count = DIV_ROUND_UP(sectors, DIRECT_INODE * SINGLE_INDIRECT_INODE);
      if(free_map_allocate (disk_inode->count, disk_inode->inode_index)){
        size_t size;
        int i;
        for (i = 0; i < disk_inode->count; i++){
          size = sectors-DIRECT_INODE*SINGLE_INDIRECT_INODE*i;
          if(size > SINGLE_INDIRECT_INODE * DIRECT_INODE)
            size = SINGLE_INDIRECT_INODE * DIRECT_INODE;
          if(!inode_create(disk_inode->inode_index[i], size*DISK_SECTOR_SIZE, level-1, is_dir)){
            success = false;
            for(i=i-1;i>=0;i--)
              inode_delete(disk_inode->inode_index[i]);
            break;
          }
        }
        if(i == disk_inode->count){
          cache_write (sector, disk_inode, 0, DISK_SECTOR_SIZE);
          success = true; 
        }
      }
    }

    free (disk_inode);
  }
  return success;
}

void inode_delete(disk_sector_t sector){
  struct inode_disk *disk_inode;
  disk_inode = calloc (1, sizeof *disk_inode);
  if(disk_inode != NULL){
    cache_read(sector, disk_inode, 0, DISK_SECTOR_SIZE);
    if(disk_inode->level == 0)
      free_map_release(disk_inode->inode_index, disk_inode->count);
    else{
      size_t i;
      for(i=0;i<disk_inode->count;i++)
        inode_delete(disk_inode->inode_index[i]);
    }
    free(disk_inode);
  }
  free_map_release(&sector, 1);
}

bool
inode_growth (struct inode_disk *disk_inode, disk_sector_t sector, off_t length, int level, bool is_dir)
{
  bool success = true;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  size_t sectors = bytes_to_sectors (length);
  disk_inode->length = length;
  disk_inode->level = level;
  disk_inode->is_dir = is_dir;
  size_t old_count = disk_inode->count;
  if(level == 0){ // direct
    disk_inode->count = sectors;
    if (free_map_allocate (disk_inode->count - old_count, disk_inode->inode_index + old_count)){
      static char zeros[DISK_SECTOR_SIZE];
      size_t i;

      cache_write (sector, disk_inode, 0, DISK_SECTOR_SIZE);
      for (i = old_count; i < disk_inode->count; i++) 
        cache_write (disk_inode->inode_index[i], zeros, 0, DISK_SECTOR_SIZE); 
    } 
    else
      success = false;
  }
  else if(level == 1){ // sigle indirect
    disk_inode->count = DIV_ROUND_UP(sectors, DIRECT_INODE);
    size_t size = sectors-DIRECT_INODE*(old_count-1);
    if(size > DIRECT_INODE)
      size = DIRECT_INODE;    
    if(old_count!=0){
      struct inode_disk *new_disk_inode = calloc(1, sizeof(struct inode_disk));
      if(new_disk_inode == NULL)
        return false;
      cache_read(disk_inode->inode_index[old_count-1], new_disk_inode, 0, DISK_SECTOR_SIZE);
      success &= inode_growth(new_disk_inode, disk_inode->inode_index[old_count-1], size*DISK_SECTOR_SIZE, level-1, is_dir);
      free(new_disk_inode);
    }
    if (success && free_map_allocate (disk_inode->count - old_count, disk_inode->inode_index + old_count)){
      cache_write (sector, disk_inode, 0, DISK_SECTOR_SIZE);
      int i;
      for (i = old_count; i < disk_inode->count; i++){
        size = sectors - DIRECT_INODE*i;
        if(size > DIRECT_INODE)
          size = DIRECT_INODE;
        if(!inode_create(disk_inode->inode_index[i], size*DISK_SECTOR_SIZE, level-1, is_dir))
          return false;
      }
    }
    else
      success = false;
  }
  else{ // double indirect
    disk_inode->count = DIV_ROUND_UP(sectors, DIRECT_INODE * SINGLE_INDIRECT_INODE);
    size_t size = sectors-DIRECT_INODE*SINGLE_INDIRECT_INODE*(old_count-1);
    if(size > SINGLE_INDIRECT_INODE * DIRECT_INODE)
      size = SINGLE_INDIRECT_INODE * DIRECT_INODE;
    if(old_count!=0){
      struct inode_disk *new_disk_inode = calloc(1, sizeof(struct inode_disk));
      if(new_disk_inode == NULL)
        return false;
      cache_read(disk_inode->inode_index[old_count-1], new_disk_inode, 0, DISK_SECTOR_SIZE);
      success &= inode_growth(new_disk_inode, disk_inode->inode_index[old_count-1], size*DISK_SECTOR_SIZE, level-1, is_dir);
      free(new_disk_inode);
    }
    if (success && free_map_allocate (disk_inode->count - old_count, disk_inode->inode_index + old_count)){
      cache_write (sector, disk_inode, 0, DISK_SECTOR_SIZE);
      int i;
      for (i = old_count; i < disk_inode->count; i++){
        size = sectors-DIRECT_INODE*SINGLE_INDIRECT_INODE*i;
        if(size > SINGLE_INDIRECT_INODE * DIRECT_INODE)
          size = SINGLE_INDIRECT_INODE * DIRECT_INODE;
        if(!inode_create(disk_inode->inode_index[i], size*DISK_SECTOR_SIZE, level-1, is_dir))
          return false;
      }
    }
    else
      success = false;
  }
  return success;
}
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read (inode->sector, &inode->data, 0, DISK_SECTOR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  bool flag = inode->sector == 4023;
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          inode_delete(inode->sector);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  if(offset>=inode->data.length)
    return 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_read (sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  if (inode->deny_write_cnt)
    return 0;
  if(offset + size > inode->data.length){ //growth
    if(!inode_growth(&inode->data, inode->sector, offset + size, INODE_MAX_LEVEL, inode->data.is_dir))
      return 0;
    inode->data.length = offset + size;
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      
      cache_write (sector_idx, buffer + bytes_written, sector_ofs, chunk_size); 

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool inode_is_dir (struct inode *inode){
  return inode->data.is_dir;
}

int inode_number(struct inode *inode){
  return inode->sector;
}

int inode_parent_number(struct inode *inode){
  return inode->data.parent_sector;
}

void inode_set_parent(struct inode *inode, disk_sector_t parent_sector){
  inode->data.parent_sector = parent_sector;
  cache_write(inode->sector, &inode->data, 0, DISK_SECTOR_SIZE);
}

bool inode_removed(struct inode *inode){
  return inode->removed;
}
