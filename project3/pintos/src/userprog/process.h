#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#define PMT_SIZE 21
#define FMT_SIZE 11
#define CFL_SIZE 11
#define FD_SIZE 6
#define FDS_SIZE 6
#define WCL_SIZE 21
#define NAME_SIZE 16

#include "vm/page.h"

typedef int pid_t;

/* Check whether user process is started. */
bool is_start;
/* Number of running process. */
int proc_cnt;
/* Total number of files opened so far. */
int fd_cnt;
struct lock exit_lock;
struct lock exec_lock;
struct lock wait_lock;
struct lock create_lock;
struct lock remove_lock;
struct lock open_lock;
struct lock filesize_lock;
struct lock read_lock;
struct lock write_lock;
struct lock seek_lock;
struct lock tell_lock;
struct lock close_lock;
struct lock filesys_lock;

/* File Mapping Table. */
struct file_mapping_table
	{
		int fd;
		int pmt_idx;
		int cfl_idx;
	};
struct file_mapping_table fmt[FMT_SIZE];

struct file_descriptor_set
	{
		int fd;
		struct file *file_ptr;
	};
/* Process Management Table. */
struct process_management_table
	{
	  bool is_filled;
	  tid_t thread_id;							/* process_execute () */
		char name[NAME_SIZE];								/* process_execute () */
	  int ret_val;									/* exit */	
	  unsigned short waiting_child_cnt;				/* exit,wait */
		int waiting_child_list[WCL_SIZE];
	  bool is_dead;									/* exit */
	  struct thread *parent_thread;	/* process_execute () */
	  struct file_descriptor_set fds[FDS_SIZE];							/* open,close */
	};
struct process_management_table pmt[PMT_SIZE];

struct current_file_list
	{	
		bool is_filled;
		bool is_opened;
		struct file *file_ptr;
		int fd[FD_SIZE];
		char file_name[NAME_SIZE];						/* open */
//		char player_name[NAME_SIZE];					/* open */
		struct lock file_lock;
	};
struct current_file_list cfl[CFL_SIZE];

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

bool check_wcl (int idx, tid_t child_tid);
void insert_wcl (int idx, tid_t child_tid);
void remove_wcl (int idx, tid_t child_tid);
void insert_cfl_fd (int fd, struct file *file);
void remove_cfl_fd (int fd, struct file *file);
int search_idx (tid_t tid);
int search_empty (void);
int search_fds_idx (int idx, int fd);
int search_fds_empty (int idx);
int search_cfl_idx_same_fn_filename (const char *fn);
int search_cfl_empty (void);
int search_fmt_idx (int fd);
int search_fmt_empty (void);
void discontinue_until_acquire_lock (struct lock *);
#endif /* userprog/process.h */
