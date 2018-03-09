#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
    pid_t pid;
    if (!fork()){
        cpuio_bound(10, 1, 0);
    }else {
        if (!fork()){
            cpuio_bound(10, 0, 2);
        }else {
            if (!fork()){
                cpuio_bound(10, 1, 9);
            }else{
                if (!fork()){
                    cpuio_bound(10, 9, 1);
                }else {
                    if (!fork()){
                        cpuio_bound(10, 1, 1);
                    }else {
                        if (!fork()){
                            cpuio_bound(10, 3, 1);
                        }else {
                            while(wait(NULL) != -1){
                        }
                    }
                }
            }
        }
    }
    return 0;
}
