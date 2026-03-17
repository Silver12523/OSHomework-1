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
SYSCALL_DEFINE2(get_process_runtime, pid_t, target_pid, int __user *, user_result) {
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

四、打包上传

打包为deb文件


