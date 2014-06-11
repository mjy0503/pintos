#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

#define INODE_MAX_LEVEL 2
struct bitmap;

void inode_init (void);
bool inode_create (disk_sector_t, off_t, int, bool);
void inode_delete(disk_sector_t sector);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
bool inode_is_dir (struct inode *);
int inode_number(struct inode *inode);
int inode_parent_number(struct inode *inode);
void inode_set_parent(struct inode *inode, disk_sector_t parent_sector);
bool inode_removed(struct inode *inode);

#endif /* filesys/inode.h */
