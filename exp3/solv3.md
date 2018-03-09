# 0x00 实验内容  
1. 此次实验的基本内容是：在Linux 0.11上添加两个系统调用，并编写两个简单的应用程序测试它们。
    - iam()
        第一个系统调用是iam()，其原型为：
            `int iam(const char * name);`

        完成的功能是将字符串参数name的内容拷贝到内核中保存下来。要求name的长度不能超过23个字符。返回值是拷贝的字符数。如果name的字符个数超过了23，则返回“-1”，并置errno为EINVAL。

        在kernel/who.c中实现此系统调用。

    - whoami()

        第二个系统调用是whoami()，其原型为：

        `int whoami(char* name, unsigned int size);`

        它将内核中由iam()保存的名字拷贝到name指向的用户地址空间中，同时确保不会对name越界访存（name的大小由size说明）。返回值是拷贝的字符数。如果size小于需要的空间，则返回“-1”，并置errno为EINVAL。

        也是在kernal/who.c中实现。

2. 测试程序

    运行添加过新系统调用的Linux 0.11，在其环境下编写两个测试程序iam.c和whoami.c。最终的运行结果是：

    ```
    $ ./iam lizhijun  
    $ ./whoami
    lizhijun
    ```

# 0x01 原理  
## 为何要有系统调用？  
- 内存中存在内核空间和用户空间，硬件设计上将内核空间与用户空间进行了隔离，一个进程中处于用户空间的程序指令不能直接访问读取内核空间的数据。这里要引入特权级（**CPL：当前特权级**和**DPL：描述符特权级**）的概念，某一个应用程序要访问某个段的内容时，要求 DPL >= CPL。
- 那么用户态的程序想要如何能访问内核态呢？硬件提供给我们一些进入内核的方法，对于 intel x86 就可以借助于`int 0x80`指令，它提供了后续的系统调用的方法，系统调用的接口程序（比如一些库函数）包含了`int 0x80`
## 系统调用的基本步骤：  
1. 应用程序调用库函数（API）;
    ```cpp
    #define __LIBRARY__
    #include "unistd.h"

    _syscall1(func_type, func_name, value_type, value);
    ```
2. API将系统调用号存入EAX，然后通过中断调用使系统进入内核;
    ```c++
    // unistd.h 中的 _syscall1
    #define _syscall1(type,name,atype,a) \
    type name(atype a) \
    { \
    long __res; \
    __asm__ volatile ("int $0x80" \
        : "=a" (__res) \
        : "0" (__NR_##name),"b" ((long)(a))); \
    if (__res >= 0) \
        return (type) __res; \
    errno = -__res; \
    return -1; \
    }
    ``` 
3. 内核中的中断处理函数根据系统调用号，调用对应的内核函数（系统调用）;  
    ```makefile
    _system_call:
        cmpl $nr_system_calls-1,%eax
        ja bad_sys_call
        push %ds
        push %es
        push %fs
        pushl %edx
        pushl %ecx		# push %ebx,%ecx,%edx as parameters
        pushl %ebx		# to the system call
        movl $0x10,%edx		# set up ds,es to kernel space
        mov %dx,%ds
        mov %dx,%es
        movl $0x17,%edx		# fs points to local data space
        mov %dx,%fs
        call _sys_call_table(,%eax,4) # 调用对应的内核函数
        pushl %eax      # 将返回值存入 EAX
        movl _current,%eax
        cmpl $0,state(%eax)		# state
        jne reschedule
        cmpl $0,counter(%eax)		# counter
        je reschedule
    ```
4. 系统调用完成相应功能，将返回值存入EAX，返回到中断处理函数;
5. 中断处理函数返回到API中；
6. API将EAX返回给应用程序。

## 应用程序如何调用系统调用  
1. 把系统调用的编号存入EAX;
2. 把函数参数存入其它通用寄存器;
3. 触发0x80号中断（int 0x80)

## 从“int 0x80”进入内核函数  
1. 在内核初始化时，主函数调用了sched_init()初始化函数：
    ```cpp
    void main(void)    
    {            
        ……
        time_init();
        sched_init();
        buffer_init(buffer_memory_end);
        ……
    }
    ```
2. sched_init()在kernel/sched.c中定义为：  
    ```cpp
    void sched_init(void)
    {
        ……
        set_system_gate(0x80,&system_call);
    }
    ```
3. set_system_gate是个宏，在include/asm/system.h中定义为：
    ```cpp
    \#define set_system_gate(n,addr) \
        _set_gate(&idt[n],15,3,addr)
    _set_gate的定义是：
    \#define _set_gate(gate_addr,type,dpl,addr) \
    __asm__ ("movw %%dx,%%ax\n\t" \
        "movw %0,%%dx\n\t" \
        "movl %%eax,%1\n\t" \
        "movl %%edx,%2" \
        : \
        : "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
        "o" (*((char *) (gate_addr))), \
        "o" (*(4+(char *) (gate_addr))), \
        "d" ((char *) (addr)),"a" (0x00080000))
    ```
4. 以上步骤的目的是填写IDT（中断描述符表），将`system_call`函数地址写到0x80对应的中断描述符中，也就是在中断`0x80`发生后，自动调用函数`system_call`

# 0x02 实验步骤  
1. 修改`linux-0.11/include/unistd.h`：
    ```cpp
    #define __NR_iam 72
    #define __NR_whoami 73
    ```;
    在尾部添加下面两行：  
    ```cpp
    int iam(const char *name);
    int whoami(char *name, unsigned int size);
    ```
2. 修改`linux-0.11/include/linux/sys.h`：
    将`sys_iam, sys_whoami`依次追加到`sys_call_table`数组里，并在上面添加  
    ```cpp
    extern int sys_iam();
    extern int sys_whoami();
    ```
3. 将`linux-0.11/kernel/system_call.s`中的`nr_system_calls`的值改为 74（因为添加了两个系统调用函数）
4. 在 `oslab`下执行`sudo ./mount-hdc`挂载`hdc`目录，将修改的`unistd.h`和`sys.h`拷贝到`hdc`目录下的相应位置（也可省去1、2步，进入虚拟机后再进行修改）
5. 进入`linux-0.11/kernel/`目录，创建一个`who.c`，完成“在内核中实现函数`sys_iam()`和`sys_whoami()`”:
    ```cpp
    /*
    *who.c
    */

    #include<asm/segment.h> //引用 get_fs_byte 和 put_fs_byte 函数
    #include<linux/kernel.h> //引用 printk 函数
    #include<errno.h> //引入 errno 和 EINVAL

    // 存放用户名
    char namezone[24] = {0};
    // 记录用户名长度
    int namelen = 0;

    int sys_iam(const char *name) {
        int len_limit = 0;

        // 用户名长度限定不超过 23
        while (get_fs_byte(name+len_limit) != '\0' && len_limit < 24) {
            len_limit ++;
        }
        if (len_limit > 23) {
            errno = EINVAL;
            return -1;
        }else {
            // 调用 get_fs_byte 函数，将用户空间的数据存放到内核空间里去
            for (i = 0;i < len_limit; i++) {
                namezone[i] = get_fs_byte(name+i);
            }
            namelen = len_limit;
            return namelen;
        }
    }

    int sys_whoami(char *name, unsigned int size) {
        int i;
        if (size < namelen) {
            errno = EINVAL;
            return -1;
        }else {
            for (i = 0; i < namelen; i++) {
                // 调用 put_fs_byte 函数，准备将内核空间的数据存放到用户空间中去
                put_fs_byte(namezone[i], (name+i));
            }
            return namelen;
        }
    }

    ```
6. 按照实验提示上的`Makefile`的规则，进行相应修改，然后在`linux-0.11`目录下执行`make all`，之后`run`进入虚拟机
7. 在虚拟机中选择一个目录（这里选择用`/usr/root/`目录），分别编写`iam.c`和`whoami.c`文件，内容如下：  
    ```cpp
    /*
    *iam.c
    */
    #define __LIBRARY__
    #include<unistd.h>

    _syscall1(int, iam, const char*, name); // iam() 在用户空间的接口函数

    int main(int argc, char* argv[]) {
        // 读取命令行参数，并调用 iam()
        iam(argv[1]);
        return 0;
    }

    ```
    ```cpp
    /*
    *whoami.c
    */
    #define __LIBRARY__
    #include<unistd.h>

    _syscall2(int, whoami, char*, name, unsigned int, size); // whoami() 在用户空间的接口函数  

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
    ```
8. 终端下依次执行`gcc -o iam iam.c -Wall`和`gcc -o whoami whoami.c -Wall`，执行`./testlab2.sh`，得到结果  
    ![](http://ww1.sinaimg.cn/large/a105112bly1fmwpero5j6j20if04tmxh.jpg)

# 0x03 遇到的问题及解决办法  
- 在理解原理和实验操作上都遇到过很多坑，整个实验反反复复做了很多遍，参考其他人的解决方案，才最终把实验做出来，下面就记录一下我在实验过程中遇到的问题。
1. 在写`who.c`时比较困惑的是通过`get_fs_byte`函数如何将用户空间的数据保存到内核空间里去，后来参考了网上一些同学的代码，在内核空间里定义一个数组，然后将用户空间的数据存储在这个数组里，之后调用`whoami`时便从该数组中读取;
2. 一开始在`iam.c`程序中，参考`close.c`的写法，仅仅调用了`_syscall1`函数，编译时，gcc 报出如下错误：  
    `/usr/lib/crt0.o: Undefined symbol _main referenced from text segment`  
    上网查找了原因，是代码中缺少`main`函数，因此需要定义一个`main`函数，在里面调用`iam(argv[1])`函数（`argv[1]`读取命令行参数）。
3. 
    - `who.c`中的`sys_whoami`一开始使用`printk`打印出数据，觉得应该这样就满足了实验要求;   
    - 在`whoami.c`中，需要给`whoami`函数传递两个参数，第二个好理解，`unsigned int size`为了读取完整的用户名，第一个不明白什么意思（之后会说），就随便放上个字符串吧（`""`）。  
    - 于是编译，运行，发现确实能够打出`iam`定义的名字（有点小兴奋），然后就重新编译，执行`./testlab2.sh`， **final score：zero** ...这就很尴尬，继续找原因;
    - 查看了`testlab2.sh`的脚本，是通过`score=$(./whoami)`进行判断，自己在 bash 中尝试一遍，调用`whoami`发现虽然能够正常打印出用户名（`sys_whoami`里的`printk(namezone)`），但是`echo $score`为空。  
    - 原来，实验提示中已经提到`printk`是内核函数，因此，打印出来的东西是在内核空间，`bash`在用户空间里，结合`put_fs_byte`就可以知道，我们可以在`whoami.c`（用户空间）中定义一个数组，并作为第一个参数传递给`whoami()`函数，这样，适当修改`who.c`中的`sys_whoami`部分，根据其返回值，我们就可以在`whoami.c`中添加`printf(username)`（在用户空间中打印）。