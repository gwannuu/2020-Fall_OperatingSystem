#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#define PMT_SIZE 20
#define FDS_SIZE 10

typedef int pid_t;

/* Number of running process. */
int proc_cnt;
/* Total number of files opened so far. */
int fd_cnt;

/* File Descripter Set. */
struct file_descripter_set
{
  bool is_filled;
  int fd;
  struct file *file_ptr;
};

/* Process Management Table. */
struct process_management_table
{
  bool is_filled;
  tid_t thread_id;
  int ret_val;
  bool is_waiting_child;
  struct thread *parent_thread;
  struct file_descripter_set fds[FDS_SIZE];
};
struct process_management_table pmt[PMT_SIZE];

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

int search_idx (tid_t);
int search_blank (void);
int search_fds_idx (int, int);
int search_fds_blank (int);
#endif /* userprog/process.h */
