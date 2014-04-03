#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <string.h>

#define pid_t int

static void syscall_handler (struct intr_frame *);

struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int selector = *(int *)(f->esp);
  if(selector == SYS_HALT){
  }
  else if(selector == SYS_EXIT){
  }
  else if(selector == SYS_EXEC){
  }
  else if(selector == SYS_WAIT){
  }
  else if(selector == SYS_CREATE){
  }
  else if(selector == SYS_REMOVE){
  }
  else if(selector == SYS_OPEN){
  }
  else if(selector == SYS_FILESIZE){
  }
  else if(selector == SYS_READ){
  }
  else if(selector == SYS_WRITE){
    int fd;
    void *buffer;
    unsigned size;
    memcpy(&fd, f->esp+4, sizeof(int));
    memcpy(&buffer, f->esp+8, sizeof(void *));
    memcpy(&size, f->esp+12, sizeof(unsigned));
    f->eax = sys_write(fd, buffer, size);
  }
  else if(selector == SYS_SEEK){
  }
  else if(selector == SYS_TELL){
  }
  else if(selector == SYS_CLOSE){
  }
  else if(selector == SYS_MMAP){
  }
  else if(selector == SYS_MUNMAP){
  }
  else if(selector == SYS_CHDIR){
  }
  else if(selector == SYS_MKDIR){
  }
  else if(selector == SYS_READDIR){
  }
  else if(selector == SYS_ISDIR){
  }
  else if(selector == SYS_INUMBER){
  }

  thread_exit ();
}

void sys_halt (void){}
void sys_exit (int status){}
pid_t exec (const char *cmd_line){
  return 0;
}
int sys_wait (pid_t pid){
  return 0;
}
bool sys_create (const char *file, unsigned initial_size){
  return false;
}
bool sys_remove (const char *file){
  return false;
}
int sys_open (const char *file){
  return 0;
}
int sys_filesize (int fd){
  return 0;
}
int sys_read (int fd, void *buffer, unsigned size){
  return 0;
}
int sys_write (int fd, const void *buffer, unsigned size){
  if(fd == STDOUT_FILENO){
    putbuf(buffer, size);
    return size;
  }
  lock_acquire(&file_lock);
  lock_release(&file_lock);
}
void sys_seek (int fd, unsigned position){}
unsigned sys_tell (int fd){
  return 0;
}
void sys_close (int fd){}
