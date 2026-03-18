#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h> 
#include <linux/sched.h> 
#include <linux/pid.h>
#include <linux/time64.h>

// 定义一个有 2 个参数的系统调用
// 参数 1: 目标进程的 PID
// 参数 2: 用户态的一个 u64 指针，用来接收结果
SYSCALL_DEFINE2(syscall_customize, pid_t, target_pid, u64 __user *, user_result) {
    struct task_struct *task;
    struct task_struct *group_leader; // 进程主线程
    u64 runtime_us;                   
    ktime_t run_time;                 

    
    rcu_read_lock();
    // 找到目标PID对应的任意线程
    task = pid_task(find_vpid(target_pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock(); 
        return -ESRCH;     
    }

    // 获取进程主线程，计算总运行时间
    group_leader = task->group_leader;                
    run_time = ktime_sub(ktime_get(), group_leader->start_time); 
    runtime_us = ktime_to_us(run_time);               

    rcu_read_unlock();

    if (copy_to_user(user_result, &runtime_us, sizeof(u64))) {
        return -EFAULT; 
    }

    printk(KERN_INFO "PID %d 主线程启动时间：%llu ns，运行时间：%llu us",
           target_pid, (u64)ktime_to_ns(group_leader->start_time), runtime_us);

    return 0; 
}

