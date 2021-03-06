#include <stdbool.h>
#include "threads/synch.h"
#include "filesys/directory.h"
#include <list.h>
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define pid_t int
#define mapid_t int

struct file_fd{
  int fd;                 /* number of file descriptor */
  struct file *file;      /* opened file */
  struct dir *dir;        /* opened directory */
  struct list_elem elem;
};

struct lock file_lock;

void syscall_init (void);

void sys_halt (void);
void sys_exit (int status);
pid_t sys_exec (const char *cmd_line);
int sys_wait (pid_t pid);
bool sys_create (const char *file, unsigned initial_size);
bool sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
int sys_read (int fd, void *buffer, unsigned size);
int sys_write (int fd, const void *buffer, unsigned size);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);
mapid_t sys_mmap(int fd, void *addr);
void sys_munmap(mapid_t mapping);
bool sys_chdir(const char *dir);
bool sys_mkdir(const char *dir);
bool sys_readdir(int fd, char *name);
bool sys_isdir(int fd);
int sys_inumber(int fd);

#endif /* userprog/syscall.h */
