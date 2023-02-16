#include "defs.h"
.extern trap_handler
.section .text.entry
    .align 2
    .globl _traps 
_traps:
    # 如果sp不是内核态的栈，则需要切换
    li t0, VM_START
    bgeu sp, t0, _trap_sd
    csrr t0, sscratch
    csrw sscratch, sp
    mv  sp, t0
_trap_sd:
    # 1. save 32 registers and sepc to stack
    addi sp, sp, -272
	sd x1, 264(sp)
	# sd x2, 256(sp)
	sd x3, 248(sp)
	sd x4, 240(sp)
	sd x5, 232(sp)
	sd x6, 224(sp)
	sd x7, 216(sp)
	sd x8, 208(sp)
	sd x9, 200(sp)
	sd x10, 192(sp)
	sd x11, 184(sp)
	sd x12, 176(sp)
	sd x13, 168(sp)
	sd x14, 160(sp)
	sd x15, 152(sp)
	sd x16, 144(sp)
	sd x17, 136(sp)
	sd x18, 128(sp)
	sd x19, 120(sp)
	sd x20, 112(sp)
	sd x21, 104(sp)
	sd x22, 96(sp)
	sd x23, 88(sp)
	sd x24, 80(sp)
	sd x25, 72(sp)
	sd x26, 64(sp)
	sd x27, 56(sp)
	sd x28, 48(sp)
	sd x29, 40(sp)
	sd x30, 32(sp)
	sd x31, 24(sp)
    csrr t0, sepc
    sd t0, 16(sp)
    csrr t1, sstatus
    sd t1, 8(sp)
    csrr t2, sscratch
    sd t2, 0(sp)

    # 2. call trap_handler
    csrr a0, scause
    csrr a1, stval
    mv a2, sp
    call trap_handler
    .global __ret_from_fork
__ret_from_fork:
    # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack
	ld x1, 264(sp)
	# ld x2, 256(sp)
	ld x3, 248(sp)
	ld x4, 240(sp)
	ld x5, 232(sp)
	ld x6, 224(sp)
	ld x7, 216(sp)
	ld x8, 208(sp)
	ld x9, 200(sp)
	ld x10, 192(sp)
	ld x11, 184(sp)
	ld x12, 176(sp)
	ld x13, 168(sp)
	ld x14, 160(sp)
	ld x15, 152(sp)
	ld x16, 144(sp)
	ld x17, 136(sp)
	ld x18, 128(sp)
	ld x19, 120(sp)
	ld x20, 112(sp)
	ld x21, 104(sp)
	ld x22, 96(sp)
	ld x23, 88(sp)
	ld x24, 80(sp)
	ld x25, 72(sp)
	ld x26, 64(sp)
	ld x27, 56(sp)
	ld x28, 48(sp)
	ld x29, 40(sp)
	ld x30, 32(sp)
	ld x31, 24(sp)
    ld t0, 16(sp)
    csrw sepc, t0
    ld t1, 8(sp)
    csrw sstatus, t1
    ld t2, 0(sp)
    csrw sscratch, t2
    addi sp, sp, 272

    li t0, VM_START
    csrr t1, sepc
    bgeu t1, t0, _trap_end
    csrr t0, sscratch
    csrw sscratch, sp
    mv  sp, t0
_trap_end:
    # 4. return from trap
    sret

    .globl __dummy
__dummy:
    csrr t0, sscratch
    csrw sscratch, sp
    mv  sp, t0
    sret

    .globl __switch_to
__switch_to:
    # save state to prev process
    sd ra, 48(a0)
    sd sp, 56(a0)
    sd s0, 64(a0)
    sd s1, 72(a0)
    sd s2, 80(a0)
    sd s3, 88(a0)
    sd s4, 96(a0)
    sd s5, 104(a0)
    sd s6, 112(a0)
    sd s7, 120(a0)
    sd s8, 128(a0)
    sd s9, 136(a0)
    sd s10, 144(a0)
    sd s11, 152(a0)
    csrr s0, sepc
    csrr s1, sstatus
    csrr s2, sscratch
    sd s0, 160(a0)
    sd s1, 168(a0)
    sd s2, 176(a0)

    # restore state from next process
    ld ra, 48(a1)
    ld sp, 56(a1)
    ld s0, 64(a1)
    ld s1, 72(a1)
    ld s2, 80(a1)
    ld s3, 88(a1)
    ld s4, 96(a1)
    ld s5, 104(a1)
    ld s6, 112(a1)
    ld s7, 120(a1)
    ld s8, 128(a1)
    ld s9, 136(a1)
    ld s10, 144(a1)
    ld s11, 152(a1)
    ld t0, 160(a1)
    ld t1, 168(a1)
    ld t2, 176(a1)
    csrw sepc, t0
    csrw sstatus, t1
    csrw sscratch, t2

    li t4, 8
    slli t4, t4, 60
    ld t3, 184(a1)
    srli t3, t3, 12
    add t3, t3, t4
    csrw satp, t3
    sfence.vma zero, zero
    fence.i
    ret
