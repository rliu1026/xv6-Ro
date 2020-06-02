// Written by UW Madison CS 537 Spring 2020 faculty and staff.

#include "types.h"
#include "user.h"

int sem_id;

void
func(void *arg1, void *arg2)
{
  printf(1, "child should finish first\n");
  sem_post(sem_id);
  exit();
}

int main(int argc, char *argv[]){
    int arg1 = 0xABCDABCD;
    int arg2 = 0xCDEFCDEF;

    int count = 0;
    if(sem_init(&sem_id,count) < 0){
        printf(1, "main: error initializing semaphore\n");
        exit();
    }

    printf(1, "got assigned sem id %d\n", sem_id);

    // Using semaphores to make parent thread wait for the child thread:

    int pid1 = thread_create(&func, &arg1, &arg2);
    if (pid1 < 0) {
      exit();
    }
    sem_wait(sem_id);
    printf(1, "parent: locked till child finishes\n");
    thread_join();
    sem_destroy(sem_id);
    printf(1, "Semaphore destroyed. \nTry sem_wait now: %d\n", sem_wait(sem_id));
    exit();
}
 