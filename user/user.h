#ifndef _USER_H_
#define _USER_H_

struct stat;
struct pstat;
enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

#include "pstat.h"

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int mknod(char*, short, short);
int unlink(char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkdir(char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int clone(void(*fcn)(void*, void*), void *arg1, void *arg2, void *stack);
int join(void **stack);
int getprocinfo(struct pstat*);
int boostproc(void);
int mprotect(void*, int);
int munprotect(void*, int);
int dump_allocated(int*, int);
int sem_init(int*, int);
int sem_wait(int);
int sem_post(int);
int sem_destroy(int);
int getfilenum(int); 

// user library functions (ulib.c)
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int thread_create(void (*)(void*, void*), void *arg1, void *arg2);
int thread_join(void);

#endif // _USER_H_

