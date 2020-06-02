#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
    char * nullptr = (char *)0;
    // After calling mprotect in the below line, 
    //  any write to the address ranging from 0x0000 to 0x1000 
    //  will trigger a page fault and terminate the process.
    mprotect(nullptr, 1);

    if (fork() == 0){
        // child process will inherit the protection bit
        // This line will triger the page fault and terminate
        printf(1, "This is child pid = %d\n", getpid());
        nullptr[1] = nullptr[1];
        printf(1, "Should have got page fault\n");
    } else {
        wait();
    }

    //nullptr[1] = nullptr[1];

    // This line will unprotect the address ranging from 0x0000 to 0x1000
    munprotect(nullptr, 1);

    // This line shouldn't trigger any page fault.
    printf(1, "This is parent pid = %d\n", getpid());
    nullptr[1] = nullptr[1];
    printf(1, "Parent has no page fault\nExit\n");
    exit();
}