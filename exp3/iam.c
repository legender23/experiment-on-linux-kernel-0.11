#define __LIBRARY__
#include<unistd.h>

static inline _syscall1(int, iam, const char*, name); // iam() 在用户空间的接口函数

int main(int argc, char* argv[]) {
    // 读取命令行参数，并调用 iam()
    iam(argv[1]);
    return 0;
}
