#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <string.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_vaddr(void *vaddr)
{
  if(!is_user_vaddr(vaddr) || vaddr < (void *)0x08048000)
    sys_exit(-1);
}

void check_bytes(void *addr, unsigned size)
{
  void *save = addr;
  unsigned i;
  for(i=0;i<size;i++){
    check_vaddr(save);
    save++;
  }
}

void check_string(void *addr)
{
  void *save = addr;
  while(1){
    check_vaddr(save);
    if(*((char *)save) == '\0') break;
    save++;
  }
}

void* get_kernel_ptr(void *vaddr)
{
  check_vaddr(vaddr);
  void *kernel_ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if(kernel_ptr == NULL)
    sys_exit(-1);
  return kernel_ptr;
}

struct file*
get_file(int fd)
{
  struct thread *t = thread_current();
  struct list_elem *e;
  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    if(fd == list_entry(e, struct file_fd, elem)->fd)
      return list_entry(e, struct file_fd, elem)->file;
  }
  return NULL;
}

static void
syscall_handler (struct intr_frame *f UNUSED)  // is_user_vaddr, >0x08048000
{
  check_bytes(f->esp, 4);
  if(pagedir_get_page(thread_current()->pagedir, f->esp) == NULL)
    sys_exit(-1);
  int selector = *(int *)(f->esp);
  switch(selector){
    case SYS_HALT:
    {
      sys_halt();
      break;
    }
    case SYS_EXIT:
    {
      int status;
      check_bytes(f->esp+4, 4);
      memcpy(&status, f->esp+4, sizeof(int));
      sys_exit(status);
      break;
    }
    case SYS_EXEC:
    {
      const char *cmd_line;
      check_bytes(f->esp+4, 4);
      memcpy(&cmd_line, f->esp+4, sizeof(char *));
//      check_string((void *)cmd_line);
      cmd_line = get_kernel_ptr((void *)cmd_line);
      f->eax = sys_exec(cmd_line);
      break;
    }
    case SYS_WAIT:
    {
      pid_t pid;
      check_bytes(f->esp+4, 4);
      memcpy(&pid, f->esp+4, sizeof(pid_t));
      f->eax = sys_wait(pid);
      break;
    }
    case SYS_CREATE:
    {
      const char *file;
      unsigned initial_size;
      check_bytes(f->esp+4, 4);
      check_bytes(f->esp+8, 4);
      memcpy(&file, f->esp+4, sizeof(char *));
      memcpy(&initial_size, f->esp+8, sizeof(unsigned));
      file = get_kernel_ptr((void *)file);
//      check_string((void *)file);
      f->eax = sys_create(file, initial_size);
      break;
    }
    case SYS_REMOVE:
    {
      char *file;
      check_bytes(f->esp+4, 4);
      memcpy(&file, f->esp+4, sizeof(char *));
      file = get_kernel_ptr((void *)file);
//      check_string((void *)file);
      f->eax = sys_remove(file);
      break;
    }
    case SYS_OPEN:
    {
      char *file;
      check_bytes(f->esp+4, 4);
      memcpy(&file, f->esp+4, sizeof(char *));
      file = get_kernel_ptr((void *)file);
//      check_string((void *)file);
      f->eax = sys_open(file);
      break;
    }
    case SYS_FILESIZE:
    {
      int fd;
      check_bytes(f->esp+4, 4);
      memcpy(&fd, f->esp+4, sizeof(int));
      f->eax = sys_filesize(fd);
      break;
    }
    case SYS_READ:
    {
      int fd;
      void *buffer;
      unsigned size;
      check_bytes(f->esp+4, 4);
      check_bytes(f->esp+8, 4);
      check_bytes(f->esp+12, 4);
      memcpy(&fd, f->esp+4, sizeof(int));
      memcpy(&buffer, f->esp+8, sizeof(void *));
      memcpy(&size, f->esp+12, sizeof(unsigned));
      check_bytes(buffer, size);
      buffer = get_kernel_ptr(buffer);
      f->eax = sys_read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      int fd;
      const void *buffer;
      unsigned size;
      check_bytes(f->esp+4, 4);
      check_bytes(f->esp+8, 4);
      check_bytes(f->esp+12, 4);
      memcpy(&fd, f->esp+4, sizeof(int));
      memcpy(&buffer, f->esp+8, sizeof(void *));
      memcpy(&size, f->esp+12, sizeof(unsigned));
      check_bytes((void *)buffer, size);
      buffer = get_kernel_ptr((void *)buffer);
      f->eax = sys_write(fd, buffer, size);
      break;
    }
    case SYS_SEEK:
    {
      int fd;
      unsigned position;
      check_bytes(f->esp+4, 4);
      check_bytes(f->esp+8, 4);
      memcpy(&fd, f->esp+4, sizeof(int));
      memcpy(&position, f->esp+8, sizeof(unsigned));
      sys_seek(fd, position);
      break;
    }
    case SYS_TELL:
    {
      int fd;
      check_bytes(f->esp+4, 4);
      memcpy(&fd, f->esp+4, sizeof(int));
      f->eax = sys_tell(fd);
      break;
    }
    case SYS_CLOSE:
    {
      int fd;
      check_bytes(f->esp+4, 4);
      memcpy(&fd, f->esp+4, sizeof(int));
      sys_close(fd);
      break;
    }
    default:
    sys_exit(-1);
    break;
  }

}

void sys_halt (void)
{
  power_off();
}

void sys_exit (int status)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_current()->process->exit_stat = status;
  thread_exit();
}

pid_t sys_exec (const char *cmd_line)
{
  pid_t ret = process_execute(cmd_line);
  return ret;
}

int sys_wait (pid_t pid)
{
  int ret = process_wait(pid);
  return ret;
}

bool sys_create (const char *file, unsigned initial_size)
{
  lock_acquire(&file_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return ret;
}

bool sys_remove (const char *file)
{
  lock_acquire(&file_lock);
  bool ret = filesys_remove(file);
  lock_release(&file_lock);
  return ret;
}

int sys_open (const char *file)
{
  lock_acquire(&file_lock);
  struct file *open_file = filesys_open(file);
  lock_release(&file_lock);
  if(open_file == NULL)
    return -1;
  struct thread *t = thread_current();
  int fd = t->maxfd;

  struct file_fd *new_file_fd = palloc_get_page(0);
  if(new_file_fd == NULL)
    return -1;

  new_file_fd->fd = fd;
  new_file_fd->file = open_file;
  list_push_back(&t->file_list, &new_file_fd->elem);
  t->maxfd++;

  return fd;
}

int sys_filesize (int fd)
{
  struct file *open_file = get_file(fd);
  if(open_file == NULL)
    return -1;
  lock_acquire(&file_lock);
  int ret = file_length(open_file);
  lock_release(&file_lock);
  return ret;
}

int sys_read (int fd, void *buffer, unsigned size)
{
  if(fd == STDIN_FILENO){
    int i;
    for(i=0;i<size;i++)
      *((uint8_t *)buffer + i) = input_getc();
    return size;
  }

  struct file *open_file = get_file(fd);
  if(open_file == NULL)
    return -1;
  lock_acquire(&file_lock);
  int ret = file_read(open_file, buffer, size);
  lock_release(&file_lock);
  return ret;
}

int sys_write (int fd, const void *buffer, unsigned size)
{
  if(fd == STDOUT_FILENO){
    putbuf(buffer, size);
    return size;
  }

  struct file *open_file = get_file(fd);
  if(open_file == NULL)
    return -1;
  lock_acquire(&file_lock);
  int ret = file_write(open_file, buffer, size);
  lock_release(&file_lock);
  return ret;
}

void sys_seek (int fd, unsigned position)
{
  struct file *open_file = get_file(fd);
  if(open_file == NULL)
    return ;
  lock_acquire(&file_lock);
  file_seek(open_file, position);
  lock_release(&file_lock);
}

unsigned sys_tell (int fd)
{
  struct file *open_file = get_file(fd);
  if(open_file == NULL)
    return -1;
  lock_acquire(&file_lock);
  unsigned ret = file_tell(open_file);
  lock_release(&file_lock);
  return ret;
}

void sys_close (int fd)
{
  struct thread *t = thread_current();
  struct file *del_file = NULL;
  struct file_fd *del_file_fd;
  struct list_elem *e;

  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    del_file_fd = list_entry(e, struct file_fd, elem);
    if(del_file_fd->fd == fd){
      del_file = del_file_fd->file;
      list_remove(e);
      palloc_free_page(del_file_fd);
      break;
    }
  }

  lock_acquire(&file_lock);
  if(del_file != NULL)
    file_close(del_file);
  lock_release(&file_lock);
}
