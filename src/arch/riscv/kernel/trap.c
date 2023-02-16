#include "printk.h"
#include "types.h"
#include "syscall.h"
#include "riscv.h"
#include "proc.h"
extern void clock_set_next_event();
extern void do_timer(void);
extern void do_page_fault(uint64 scause, uint64 stval);

void trap_handler(uint64 scause, uint64 stval, struct pt_regs *regs) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.5 节
    // 其他interrupt / exception 可以直接忽略
    
    //中断
    if ((scause >> 63) & 1) {
        //时钟中断
        if ((scause % 16) == 0x5) {
            clock_set_next_event();
            do_timer();
        }

    // 异常  
    } else {
        //系统调用
        if(scause == 8) {
            syscall(regs);
        }
        else if ((scause % 16) == 13 || (scause % 16) == 15 || (scause % 16) == 12) {
            printk("[S] Supervisor Page Fault, scause: 0x%lx, stval: 0x%lx, sepc: 0x%lx\n", scause, stval, regs->sepc);
            do_page_fault(scause, stval);
        }
    }
}