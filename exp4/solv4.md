# 0x00 实验内容
- 基于模板“process.c”编写多进程的样本程序，实现如下功能：
    - 所有子进程都并行运行，每个子进程的实际运行时间一般不超过30秒；
    - 父进程向标准输出打印所有子进程的id，并在所有子进程都退出后才退出；
- 在Linux0.11上实现进程运行轨迹的跟踪。基本任务是在内核中维护一个日志文件/var/process.log，把从操作系统启动到系统关机过程中所有进程的运行轨迹都记录在这一log文件中。
- 在修改过的0.11上运行样本程序，通过分析log文件，统计该程序建立的所有进程的等待时间、完成时间（周转时间）和运行时间，然后计算平均等待时间，平均完成时间和吞吐量。可以自己编写统计程序，也可以使用python脚本程序—— stat_log.py（在/home/teacher/目录下） ——进行统计。
- 修改0.11进程调度的时间片，然后再运行同样的样本程序，统计同样的时间数据，和原有的情况对比，体会不同时间片带来的差异。
- /var/process.log 文件的格式必须为 `pid X time`，其中：
    - pid是进程号;
    - X 可以是 **N,J,R,W** 和 **E** 中的任意一个，分别表示进程**新建(N)**、**进入就绪态(J)**、**进入运行态(R)**、**进入阻塞态(W)**和**退出(E)**；
    - time表示X发生的时间。这个时间不是物理时间，而是系统的**滴答时间(tick)**；
- 详见实验

# 0x01 实验原理
- `/var/process.log`文件用于记录进程的状态转移轨迹，状态的转移均发生在内核中，在内核状态下，`write()`功能失效，实验提示给出了`fprintk`的源码，使用方便：
    ```cpp
    fprintk(1, "The ID of running process is %ld", current->pid); //向stdout打印正在运行的进程的ID
    fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'R', jiffies); //向log文件输出跟踪进程运行轨迹
    ```
- Linux 0.11 支持四种进程转态的转移：就绪到运行、运行到就绪、运行到睡眠、睡眠到就绪。此外，还有新建和退出两种情况。该实验的关键在于找到每个状态转移的地方，并使用上述`fprintk`方法进行写文件操作。
- Linux 0.11 在`/include/linux/sched.h`中定义了五种状态：
    ```cpp
    #define TASK_RUNNING    0
    #define TASK_INTERRUPTIBLE   1
    #define TASK_UNINTERRUPTIBLE     2
    #define TASK_ZOMBIE     3
    #define TASK_STOPPED    4
    ```
    top 命令查看进程状态会发现`running、sleeping、stopped、zombie`四种状态与上述定义的状态有关


# 0x02 实验步骤
1. 编辑`/home/teacher/`目录下的`process.c`文件，该样本程序实现了一个`cpuio_bound`函数。接下来利用`fork()`建立若干个同时运行的子进程，每个子进程运行不同或相同的`cpuio_bound()`操作：
```cpp
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
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

```
2. 根据实验提示，修改`init/main.c`文件，在`move_to_user_mode()`（137行）后面（`if(!fork())`之前）添加如下代码：
    ```cpp
    setup((void *) &drive_info);
    (void) open("/dev/tty0",O_RDWR,0);    //建立文件描述符0和/dev/tty0的关联
    (void) dup(0);        //文件描述符1也和/dev/tty0关联
    (void) dup(0);        //文件描述符2也和/dev/tty0关联
    (void) open("/var/process.log",O_CREAT|O_TRUNC|O_WRONLY,0666);
    ```
    表示建立`/var/process.log`只写文件，并在`init()`中删去相应部分。
3. 修改`kernel/fork.c`中的`copy_process`，在`p->state = TASK_RUNNING`（第 131 行的）前后分别添加：
    ```cpp
    fprintk(3, "%ld\t%c\t%ld\n", p->pid, 'N', jiffies);
    ```
    和
    ```cpp
    fprintk(3, "%ld\t%c\t%ld\n", p->pid, 'J', jiffies);
    ```
4. 就绪与运行的相互转换
    修改`kernel/sched.c`中的`schedule()`部分如下：
    ```cpp
    void schedule()
    {
        ...
        if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) && (*p)->state==TASK_INTERRUPTIBLE){
            (*p)->state = TASK_RUNNING; 
            fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies);
        }
        ...
        if (task[next] != current){
            if (current->state == TASK_RUNNING)
                fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'J', jiffies);
            fprintk(3, "%ld\t%c\t%ld\n", task[next]->pid, 'R', jiffies);
        }
        switch_to(next);
    }
    ```

5. 运行到睡眠
    修改`kernel/sched.c`的`sleep_on`部分如下：
    ```cpp
    void sleep_on(struct task_struct **p)
    {
        ...
        current->state = TASK_UNINTERRUPTIBLE;
        if (current->pid != 0)
            fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
        ...
    }
    ```
    修改`kernel/sched.c`的`interruptible_sleep_on`部分如下：
    ```cpp
    void interruptible_sleep_on(struct task_struct **p)
    {
        ...
        repeat: current->state = TASK_INTERRUPTIBLE;
        if (current->pid != 0)
            fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
        ...
    }
    ```
    修改`kernel/sched.c`的`sys_pause`部分如下：
    ```cpp
    int sys_pause(void)
    {
        current->state = TASK_UNINTERRUPTIBLE;
        if (current->pid != 0)
            fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
    }
    ```
    修改`kernel/exit.c`的`sys_waitpid`部分如下：
    ```cpp
    void sys_waitpid(pid_t pid, unsigned long * stat_addr, int options)
    {
        ...
        current->state = TASK_INTERRUPTIBLE;
        if (current->pid != 0)
            fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
        ...
    }
    ```
6. 睡眠到就绪
    修改`kernel/sched.c`的`sleep_on`部分如下：
    ```cpp
    void sleep_on(struct task_struct **p)
    {
        ...
        if (tmp){
            tmp->state = 0;
            fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
        }
    }
    ```
    修改`kernel/sched.c`的`sleep_on`部分如下：
    ```cpp
    void interruptible_sleep_on(struct task_struct **p)
    {
        ...
        if (*p && *p != current){
            (**p).state = 0;
            fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
            goto repeat;
        }
        ...
        if (tmp){
            tmp->state = 0;
            fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
        }
    }
    ```
    修改`kernel/sched.c`的`wake_up`部分如下：
    ```cpp
    void wake_up(struct task_struct **p)
    {
        if (p && *p){
            if ((*p)->state != TASK_RUNNING)
                fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies);
            (**p).state = 0;
        }
    }
    ```

# 0x03 遇到的问题及解决办法
1. 在每个切换点进行 fprintk 时，没有判断进程的现有状态，这样导致的后果是可能会输出多次相同的状态，`stat_log.py`会报“进程x与前一次统计的状态值相同”之类的错误，因此，需要先判断进程的现有状态，避免重复记录相同状态值。
2. 运行态到就绪态的判断错误
    在`kernel/sched.c`的`schedule()`中
    ```cpp
    (*p)->counter = ((*p)->counter >> 1) + (*p)->priority
    ```
    前面添加了
    ```cpp
    fprintk(3, "%ld\t%c\t%ld\n", task[next]->pid, 'J', jiffies);
    ```
    一开始以为这里应当添加，这些进程的时间片用完了需要对其重新赋值，因此也应置其为就绪态。而实际上，时间片用完并不代表状态会改变（这些进程在时间片用完时的状态仍然是`TASK_RUNNING`）