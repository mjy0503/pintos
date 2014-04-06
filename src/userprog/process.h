#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

struct process_stat{
  pid_t pid;                /* process id */
  int exit_stat;            /* process exit status */
  bool is_parent_exit;      /* parent process is exit? */
  bool already_wait;        /* check wait call before */
  struct semaphore load_sema;    /* semaphore for load */
  struct semaphore wait_sema;    /* semaphore for wait */
  struct list_elem elem;    /* elem of child_list */
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
