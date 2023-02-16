#include "syscall.h"
#include "printk.h"
#include "proc.h"
#include "riscv.h"
#include "mm.h"
#include "defs.h"
#include "vm.h"
#include "string.h"
extern struct task_struct* current;
extern struct task_struct* task[NR_TASKS];
extern char __ret_from_fork[];
uint64_t sys_clone(struct pt_regs *regs) {
    /*
     1. 参考 task_init 创建一个新的 task, 将的 parent task 的整个页复制到新创建的 
        task_struct 页上(这一步复制了哪些东西?）。将 thread.ra 设置为 
        __ret_from_fork, 并正确设置 thread.sp
        (仔细想想，这个应该设置成什么值?可以根据 child task 的返回路径来倒推)
    */
    int pid = 0;
    for(int i = 2; i < NR_TASKS; i++) {
        if(task[i] == NULL) {
            pid = i;
            break;
        }
    }
    uint64 task_page = alloc_page();
    memcpy((void *)task_page, (void *)current, PGSIZE);
    task[pid] = (struct task_struct *)task_page;
    task[pid]->thread.ra = (uint64)__ret_from_fork;
    task[pid]->pid = pid;
    struct task_struct* child = task[pid];
    struct task_struct* parent = current;
    
    /*
     2. 为 child task 申请 user stack, 并将 parent task 的 user stack 
        数据复制到其中。 
    */
    uint64 stack_page = alloc_page();
    memcpy((void *)stack_page, (void *)(USER_END-(uint64)PGSIZE), PGSIZE);
    /*
     3. 为 child task 分配一个根页表，并仿照 setup_vm_final 来创建内核空间的映射,
        并建立用户态栈的映射
    */
    uint64 child_pgtbl = kvmmap();
    child->pgtbl = (uint64 *)(child_pgtbl - (uint64)PA2VA_OFFSET);
    create_mapping((uint64 *)child_pgtbl, USER_END - PGSIZE, stack_page - (uint64)PA2VA_OFFSET, PGSIZE, PTE_R | PTE_W | PTE_U | PTE_V, 1);

    /*
     4. 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
        并将其中的 a0, sp, sepc 设置成正确的值(为什么还要设置 sp?)
    */
    uint64 offset = (uint64)regs - (uint64)parent;
    struct pt_regs *child_regs = (struct pt_regs *)((uint64)child + offset);
    child_regs->a0 = 0;
    child_regs->sepc = regs->sepc + 4;
    child_regs->sstatus = regs->sstatus;
    child_regs->sscratch = regs->sscratch;
    child->thread.sp = (uint64)child_regs;
    
    /*
     5. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存
    */
    for(int i = 0; i < parent->vma_cnt; i++) {
        struct vm_area_struct *vma = &parent->vmas[i];
        if(vma->vm_flags & VM_ANONYM != 0)
            continue;
        for(uint64 va = vma->vm_start; va < vma->vm_end; va += PGSIZE) {
            uint64 *pte_entry = get_entry((uint64 *)((uint64)parent->pgtbl + (uint64)PA2VA_OFFSET), va, 0, 1);
            if(((*pte_entry) & PTE_V) == 0)
                continue;
            else {
                uint64 clone_page = alloc_page();
                memcpy((void *)clone_page, (void *)va, PGSIZE);
                create_mapping((uint64 *)child_pgtbl, va, clone_page - PA2VA_OFFSET, PGSIZE, (*pte_entry) & 0x3FF, 1);
            }
        }
    }
    /*
     6. 返回子 task 的 pid
    */
    printk("[S] New task: %d\n", pid);
    return pid;
}

void syscall(struct pt_regs *regs)
{
    switch (regs->a7)
    {
    case SYS_WRITE:
        if(regs->a0 == 1) {
            char *str = (char *)regs->a1;
            str[regs->a2] = '\0';
            regs->a0 = printk("%s", str);
        }
        break;
    case SYS_GETPID:
        regs->a0 = current->pid;
        break;
    case SYS_CLONE:
        regs->a0 = sys_clone(regs);
        break;
    default:
        break;
    }
    regs->sepc += 4;
}