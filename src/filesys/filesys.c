#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/cache.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/malloc.h"

struct disk *filesys_disk;

/* The disk that contains the file system. */
static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");
  inode_init ();
  free_map_init ();
  cache_init ();

  if (format)
    do_format ();
  
  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  cache_close ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  disk_sector_t inode_sector = 0;
  struct dir *dir = get_directory(name, true);
  if(dir == NULL)
    return false;
  char *filename = get_filename(name);
  if(*filename=='\0'){
    dir_close(dir);
    free(filename);
    return false;
  }
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, INODE_MAX_LEVEL, false)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (&inode_sector, 1);
  dir_close (dir);
  free(filename);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if(strcmp(name,"/")==0)
    return file_open(inode_open(ROOT_DIR_SECTOR));
  struct dir *dir = get_directory (name, true);
  if(dir == NULL)
    return false;
  char *filename = get_filename(name);
  if(*filename == '\0'){
    dir_close(dir);
    free(filename);
    return NULL;
  }
  struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, filename, &inode);
  dir_close (dir);
  free(filename);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = get_directory (name, true);
  if(dir == NULL)
    return false;
  char *filename = get_filename(name);
  if(*filename == '\0'){
    dir_close(dir);
    free(filename);
    return NULL;
  }
  bool success = dir_remove (dir, filename);
  dir_close (dir); 
  free(filename);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
