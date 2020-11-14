#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
static void syscall_handler (struct intr_frame *);

typedef int pid_t;

void halt (void);
void exit (int status);
/*pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
void open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);*/
int write (int fd, const void *buffer, unsigned size);
/*void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);*/

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num = *((int *)f->esp);
  switch (syscall_num) {
    case 0:
    {
      halt();
      break;
    }
    case 1:
    {
      exit(*(int *)(f->esp+4));
      break;
    }
    case 2:
//      exec();
      break;
    case 3:
//      wait();
      break;
    case 4:
//      create();
      break;
    case 5:
//      remove();
      break;
    case 6:
//      open();
      break;
    case 7:
//      filesize();
      break;
    case 8:
//      read();
      break;
    case 9:
    {
      int fd = *(int *)(f->esp + 20);
      const void *buffer = *(const char **)(f->esp + 24);
      unsigned size = *(unsigned *)(f->esp + 28);
      write(fd, buffer, size);
      break;
    }
    case 10:
//      seek();
      break;
    case 11:
//      tell();
      break;
    case 12:
//      close();
      break;
    default:
      break;
  }
  thread_exit ();
}

void halt (void) {
  shutdown_power_off();
}

void exit (int status) {
  thread_exit();
}
/*
pid_t exec (const char *cmd_line) {
  return 0;
}

int wait (pid_t pid) {
  return 0;
}

bool create (const char *file, unsigned initial_size) {
  return 0;
}

bool remove (const char *file) {
  return 0;
}

int open (const char *file) {
  return 0;
}

int filesize (int fd) {
  return 0;
}

int read (int fd, void *buffer, unsigned size){
  return 0;
}*/

int write (int fd, const void *buffer, unsigned size) {
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  else return 0;
}
/*
void seek (int fd, unsigned position) {
}

unsigned tell (int fd) {
  return 0;
}

void close (int fd) {
}*/
