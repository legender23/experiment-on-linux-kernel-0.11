#define __LIBRARY__
#include<unistd.h>

static inline _syscall2(int, whoami, char*, name, unsigned int, size); // whoami() 在用户空间的接口函数  

int main() {
    // 用于把内核空间的数据拷贝下来
    char username[64];
    if (whoami(username, 23) < 0) {
        return -1;
    }else {
        printf("%s", username);
        return 0;
    }
}
