#ifndef _SYSCALL_H
#define _SYSCALL_H
#include "riscv.h"
#include "types.h"
#define SYS_WRITE 64
#define SYS_GETPID 172
#define SYS_CLONE 220

struct pt_regs
{
    uint64 sscratch;
    uint64 sstatus;
    uint64 sepc;
    uint64 t[4];
    uint64 s[10];
    uint64 a7;
    uint64 a6;
    uint64 a5;
    uint64 a4;
    uint64 a3;
    uint64 a2;
    uint64 a1;
    uint64 a0;
};
void syscall(struct pt_regs *regs);
#endif