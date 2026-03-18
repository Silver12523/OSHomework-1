#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <sys/sysinfo.h>  
#include <string.h>       

#define __NR_syscall_customize 449  

long syscall_customize(pid_t pid, uint64_t *runtime_us) {
    if (runtime_us == NULL) {
        errno = EINVAL;
        return -1;
    }
    return syscall(__NR_syscall_customize, pid, runtime_us);
}

time_t get_process_start_time(pid_t pid) {
    char stat_path[100];
    sprintf(stat_path, "/proc/%d/stat", pid);
    FILE *fp = fopen(stat_path, "r");
    if (!fp) {
        printf("无法读取/proc/%d/stat\n", pid);
        return 0;
    }

    char buf[2048];
    unsigned long long start_jiffies;
    fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu", 
           &start_jiffies);
    fclose(fp);

    // 获取系统启动时间
    FILE *uptime_fp = fopen("/proc/uptime", "r");
    double uptime_sec;
    fscanf(uptime_fp, "%lf", &uptime_sec);
    fclose(uptime_fp);

    // 计算进程创建时间
    time_t now = time(NULL);
    time_t proc_start = now - (time_t)uptime_sec + (time_t)(start_jiffies / sysconf(_SC_CLK_TCK));
    return proc_start;
}


void test_runtime(uint32_t wait_seconds, const char *scenario_name) {
    printf("\n===== %s（等待%u秒）=====\n", scenario_name, wait_seconds);
    
    pid_t test_pid = getpid();
    uint64_t runtime_us;
    long ret;

    // 获取进程真实创建时间
    time_t proc_start = get_process_start_time(test_pid);
    if (proc_start == 0) {
        return;
    }

    // 等待指定时长
    sleep(wait_seconds);

    // 调用自定义系统调用
    ret = syscall_customize(test_pid, &runtime_us);
    if (ret != 0) {
        printf("测试失败！返回值=%ld，错误码=%d\n", ret, errno);
        return;
    }

    // 计算实际运行时间
    time_t now = time(NULL);
    uint64_t actual_sec = now - proc_start;
    uint64_t actual_us = actual_sec * 1000000;

    // 计算误差和误差率
    uint64_t error_us = (uint64_t)llabs((long long)runtime_us - (long long)actual_us);
    double error_rate = 0.0;
    if (actual_us != 0) { // 除数非0时才计算误差率
    	error_rate = 100.0 * error_us / actual_us;
    } else {
    	error_rate = 0.0; //
    }

    // 输出对比
    printf("进程PID：%d\n", test_pid);
    printf("进程创建时间：%s", ctime(&proc_start)); 
    printf("自定义系统调用返回：%" PRIu64 " 微秒（%.3f 秒）\n", 
           runtime_us, (double)runtime_us/1000000);
    printf("系统时钟计算：%" PRIu64 " 微秒（%.3f 秒）\n", 
           actual_us, (double)actual_us/1000000);
    printf("时间误差：%" PRIu64 " 微秒（误差率：%.2f%%）\n", error_us, error_rate);
}

int main() {
    // 1. 基础异常测试
    printf("===== 基础异常测试 =====\n");
    uint64_t dummy_us;
    long ret = syscall_customize(999999, &dummy_us);
    printf("无效PID(999999)测试：返回值=%ld，错误码=%d（预期非0）\n", ret, errno);

    // 2. 多时长场景测试
    test_runtime(0, "短时长场景");   
    test_runtime(10, "长时长场景"); 

    // 3. 对比系统命令
    printf("\n===== 系统命令对比=====\n");
    char cmd[200];
    sprintf(cmd, "ps -o etimes= -p %d", getpid());
    FILE *fp = popen(cmd, "r");
    double ps_sec = 0;
    fscanf(fp, "%lf", &ps_sec);
    pclose(fp);
    printf("ps命令统计运行时间：%.3f 秒（%" PRIu64 " 微秒）\n", 
           ps_sec, (uint64_t)(ps_sec * 1000000));

    return 0;
}


