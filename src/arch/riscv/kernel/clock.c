#include "printk.h"
#include "types.h"
#include "sbi.h"
// QEMU中时钟的频率是10MHz, 也就是1秒钟相当于10000000个时钟周期。
uint64 TIMECLOCK = 10000000;

uint64 get_cycles() {
    // 编写内联汇编，使用 rdtime 获取 time 寄存器中 (也就是mtime 寄存器 )的值并返回
    uint64 now;
    asm volatile ("rdtime %0" : "=r" (now) ::);
    return now;
}

void clock_set_next_event() {
    // 下一次 时钟中断 的时间点
    uint64 next = get_cycles() + TIMECLOCK;

    // 使用 sbi_ecall 来完成对下一次时钟中断的设置
    sbi_ecall(0, 0, next, 0, 0, 0, 0, 0);
} 

void time_interrupt_init() {
    __asm__ volatile(
        "rdtime t1\n"
        "addi t1, t1, 1000\n"
        "mv a6, zero\n"
        "mv a7, zero\n"
        "mv a0, t1\n"
        "ecall\n":::"t1","a7","a6","a0"
    );
}