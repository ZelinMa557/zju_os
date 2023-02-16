#include "defs.h"
.extern start_kernel
.extern setup_vm
.extern setup_vm_final
.extern mm_init
.extern early_pgtbl
.extern time_interrupt_init

    .section .text.init
    .globl _start
_start:
    la sp, boot_stack_top
    li t0, PA2VA_OFFSET
    sub sp, sp, t0
        
    call setup_vm
    call relocate
    call mm_init
    call setup_vm_final

    # set stvec = _traps
    la t0, _traps
    csrw stvec, zero
    csrw stvec, t0

    # set sie[STIE] = 1
    addi t0, zero, 32
    csrw sie, t0

    call time_interrupt_init
    
    j start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t3, PA2VA_OFFSET
    add ra, ra, t3
    add sp, sp, t3

    # set satp with early_pgtbl
    li t4, 8
    slli t4, t4, 60
    la t5, early_pgtbl
    sub t5, t5, t3
    srli t5, t5, 12
    add t6, t4, t5
    csrw satp, t6

    
    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i
    ret
        
    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 0x1000 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: