#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#define MY_SYSCALL_NUM 449

int main() {
    printf("正在测试系统调用号: %d\n", MY_SYSCALL_NUM);
    long result = syscall(MY_SYSCALL_NUM);
    if (result == -1) {
        if (errno == ENOSYS) {
            printf("错误：内核不认识这个调用号（ENOSYS）。请检查内核是否正确安装并重启。\n");
        } else {
            printf("系统调用已响应，但返回了错误码：%d\n", errno);
        }
    } else {
        printf("恭喜！系统调用执行成功，内核返回值为: %ld\n", result);
        printf("请运行 'dmesg | tail' 查看内核日志输出。\n");
    }
    return 0;
}