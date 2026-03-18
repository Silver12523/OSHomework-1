### 一、下载源码（准备工作）

实验环境：ubuntu 20.04.6，内核5.15

在kernel.org下载源码5.15（后续证明这个版本稍有点落后，容易报错）

图片见download_kernel.png

发现没有装vim，见图install_vim.png（这一步与实验内容无关但是或许可以水一段报告TT）

### 二、修改&新建文件

1. 系统调用模块文件：获取某个进程的运行时间，见图syscall_content.png

```c
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h> 
#include <linux/sched.h> 

// 定义一个有 2 个参数的系统调用
// 参数 1: 目标进程的 PID
// 参数 2: 用户态的一个 int 指针，用来接收结果
SYSCALL_DEFINE2(syscall_customize, pid_t, target_pid, int __user *, user_result) {
    struct task_struct *task;
    int runtime_seconds;
    task = find_get_task_by_vpid(target_pid);
    if (!task) {
        return -ESRCH; 
    }
    runtime_seconds = (int)(task->utime / HZ); 
    if (copy_to_user(user_result, &runtime_seconds, sizeof(int))) {
        return -EFAULT; 
    }

    printk(KERN_INFO "成功查询 PID %d 的运行时间\n", target_pid);
    return 0; 
}
```

2. 修改kernel/makefile文件，见图makefile_add.png

增加了一行obj -y += syscall_customize.o，意思是要将原始的syscall_customize.c编译为二进制.o文件

3. 修改syscall.h头文件，见图change_header.png，加了最后一行asmlinkage long sys_syscall_customize(pid_t target_pid, int __user *user_result); 让新增加的系统调用在头文件中声明

4. 修改syscall_64.tbl，见图changesyscall64.png，加了一个系统调用号449

### 三、编译

开始编译前，要配置好config，禁用一些严格审查，见图config_disable1&2.png，final_config.png。

处理完之后，见图bianyi_silent.png（一时间忘记编译的英文，又懒得给linux装输入法TT），课上老师说选择不显示提示信息编译会更快，这就是尾巴-s的作用。



编译后出现了一些问题：

问题1. 硬盘容量不足。原始容量20GB，占用容量15.5GB，编译到一半就没位置了，崩溃。

解决办法：关机，回到上一个虚拟机快照，将硬盘容量调整到50GB，重启设置生效。

问题2.由于版本略旧，出现了NFS 模块的静态断言错误，见图NFSD_error，问大模型之后建议可以绕过，因为它的本质是检测过于严格。

解决办法：绕过，执行./scripts/config --disable NFSD，重新同步配置make olddefconfig，重新开始编译。

问题3.再次提示文件系统根目录磁盘空间不足，但是已经设置了50GB，查看设置，发现只用了5.7GB。根本原因是根目录分区没有足够的余量。

解决办法：安装gparted

- 找到你的主分区（通常带 `/` 标志）。

- 如果主分区和“未分配空间（Unallocated）”之间有 Swap 分区阻挡，先右键 Swap 选择 `Disable` 或删除（之后再建）。

- 右键点击主分区 -> **Resize/Move** -> 向右拉满进度条。

- 点击工具栏的 **Apply**（勾选图标）提交修改。

发现增加的30GB完全没用上，修改之后正常运行。

修改之后输入df -h /查看当前可用容量，增加了29GB。

编译完安装，用测试文件测试即可。

### 四、打包上传

打包为deb文件

### 五、测试

最开始试着用simple_test.c测试，但是输出有问题
```
在测试系统调用号: 449
系统调用已响应，但返回了错误码：3
```
看上去像颜文字一样（...）
这是因为自定义系统调用要求传 pid_t pid + uint64_t *runtime_us，但是这段测试代码只传了调用号、没传参数（syscall(MY_SYSCALL_NUM)），内核拿到的 PID 是大概率无效的随机值，因此返回错误码 3（ESRCH = 无此进程）。

好的！接下来我又写了一段测试程序：
```c
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdlib.h>

#define __NR_syscall_customize 449  

long syscall_customize(pid_t pid, int *runtime) {
    return syscall(__NR_syscall_customize, pid, runtime);
}

int main() {
    int ret, runtime;
    pid_t self_pid = getpid();  
    pid_t invalid_pid = 999999; 

    // 获取当前进程运行时间
    printf("===== 正常测试 =====\n");
    ret = syscall_customize(self_pid, &runtime);
    if (ret == 0) {
        printf("当前进程PID：%d\n", self_pid);
        printf("自定义系统调用返回运行时间：%d 秒\n", runtime);
    } else {
        printf("正常测试失败！返回值：%d，错误码：%d\n", ret, errno);
    }

    printf("\n===== 异常测试 =====\n");
    ret = syscall_customize(invalid_pid, &runtime);
    printf("无效PID(%d)测试：返回值=%d，错误码=%d（预期非0）\n", invalid_pid, ret, errno);

    // 对比系统命令结果
    printf("\n===== 结果对比 =====\n");
    char cmd[100];
    sprintf(cmd, "ps -o etimes= -p %d", self_pid);
    FILE *fp = popen(cmd, "r");
    int ps_runtime;
    fscanf(fp, "%d", &ps_runtime);
    pclose(fp);
    printf("系统ps命令统计的运行时间：%d 秒\n", ps_runtime);
    printf("时间误差：%d 秒（≤1秒即正常）\n", abs(ps_runtime - runtime));

    return 0;
}
```

结果是：
```
===== 正常测试 =====
当前进程PID：2592
自定义系统调用返回运行时间：0 秒

===== 异常测试 =====
无效PID(999999)测试：返回值=-1，错误码=3（预期非0）

===== 结果对比 =====
系统ps命令统计的运行时间：0 秒
时间误差：0 秒（≤1秒即正常）
```
这下能看到系统调用应该是成功运行了，但是这里显示的都是0秒，太不精确，怎么回事呢？原来是系统调用中定义运行时间时使用的是int整型（int runtime_seconds;），这样我们就没办法测较短进程的运行时间了。
我们接下来把代码改成微秒级的：
```
u64 runtime_us;
```
其他地方也根据PRIu64修改。测试一下发现没什么问题之后，我决定试一试不同时长复杂一点的进程，修改了一下代码再做测试：
```
===== 基础异常测试 =====
无效PID(999999)测试：返回值=-1，错误码=3（预期非0）

===== 短时长场景（等待0秒）=====
进程PID：3288
自定义系统调用返回：159 微秒（0.000 秒）
系统时钟计算：157891 微秒（0.158 秒）
时间误差：157732 微秒...
===== 长时长场景（等待10秒）=====
进程PID：3288
自定义系统调用返回：13577 微秒（0.014 秒）
系统时钟计算：13373430 微秒（13.373 秒）
时间误差：13359853 微秒...
===== 系统命令对比=====
ps命令高精度统计：14.140 秒
```
等等？！之前测0秒的时候看不出来，我们自己写的系统调用计算的时间怎么比真实的短了这么多？这不对劲————
原来是因为我们写的系统调用计算的进程用时是 task->start_time（进程创建时间）到当前的时间差，但task->start_time不是进程 “启动时间”，而是线程创建时间。内核中 syscall() 调用可能触发轻量级线程创建，导致 start_time 是 “系统调用执行时的线程时间”，而非进程启动时间。我们的系统调用返回值其实是当前线程的运行时间，不是整个进程的运行时间。我们要从task->group_leader->start_time 获取进程主线程的启动时间。

先前的测试代码中误将「程序启动时间」当作「进程创建时间」，我们改成从 /proc/pid/stat 读取进程实际创建时间，而非 time(NULL)。

改完的代码就是syscall_customize.c和test_syscall.c啦。

进行了测试，结果算是正常：
```
===== 基础异常测试 =====
无效PID(999999)测试：返回值=-1，错误码=3（预期非0）

===== 短时长场景（等待0秒）=====
进程PID：2629
进程创建时间：Wed Mar 18 04:39:38 2026
自定义系统调用返回：1565 微秒（0.002 秒）
系统时钟计算：0 微秒（0.000 秒）
时间误差：1565 微秒（误差率：0.00%）

===== 长时长场景（等待10秒）=====
进程PID：2629
进程创建时间：Wed Mar 18 04:39:38 2026
自定义系统调用返回：10002418 微秒（10.002 秒）
系统时钟计算：10000000 微秒（10.000 秒）
时间误差：2418 微秒（误差率：0.02%）

===== 系统命令对比=====
ps命令统计运行时间：10.000 秒（10000000 微秒）
```
接下来使用strace跟踪测试。
先使用命令strace -o test_syscall_full.log -tt -T -f ./test_syscall，结果出来的太长了，难以提取有效信息。（详见test_syscall_full.txt)
筛选只和syscall_customize.c相关的，使用命令strace -o strace_log.txt -tt -T -e trace=449 ./test_syscall，出来的只有短短几行。具体内容和说明详见strace_log.txt。（这里对应的test_syscall输出就是上面PID=2629那个。
