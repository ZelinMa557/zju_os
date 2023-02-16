#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "defs.h"
#include "printk.h"
#include "riscv.h"
#include "string.h"
#include "vm.h"
#include "elf.h"
//arch/riscv/kernel/proc.c
extern void __dummy();

extern void __switch_to(struct task_struct* prev, struct task_struct* next);

extern unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern char uapp_start[];
extern char _sbss[];

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

static uint64_t load_program(struct task_struct* task, uint64* pgtbl) {
    static const char magic[16] = { 0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)uapp_start;
    for (int i = 0; i < 16; i++) {
        if (magic[i] != ehdr->e_ident[i]) {
            printk("Not valid ELF file, load fail!\n");
            return -1;
        }  
    }
    
    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;
    Elf64_Phdr* phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // do mapping
            int flags = 0;
            if (phdr->p_flags & PF_R) flags |= VM_R_MASK;
            if (phdr->p_flags & PF_W) flags |= VM_W_MASK;
            if (phdr->p_flags & PF_X) flags |= VM_X_MASK;
            do_mmap(task, phdr->p_vaddr, phdr->p_memsz, flags, phdr->p_offset, phdr->p_filesz);
        }
    }

    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
}

void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    idle = (struct task_struct *)alloc_page();
    idle->state = TASK_RUNNING;
    idle->thread.sp = (uint64)idle + PGSIZE;
    idle->pid = 0;
    idle->counter = 0;
    idle->priority = 0;

    idle->thread.sscratch = 0;
    idle->pgtbl = (uint64 *)(((uint64)swapper_pg_dir - PA2VA_OFFSET) >> 12) + (8UL << 60);
    current = idle;
    task[0] = idle;
    

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该线程在线程数组中的下标。
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    for (int i = 1; i < NR_TASKS; i++) {
        if(i > 1) {
            task[i] = NULL;
            continue;
        }
        task[i] = (struct task_struct *)alloc_page();
        task[i]->state = TASK_RUNNING;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;
        task[i]->thread.ra = (uint64)__dummy;
        task[i]->counter = 0;
        task[i]->priority = rand();
        task[i]->pid = i;

        task[i]->thread.sscratch = USER_END;
        uint64 sstatus = csr_read(sstatus);
        sstatus &= ~(1UL<<8);
        sstatus |= ((1UL<<5) | (1UL<<18));
        task[i]->thread.sstatus = sstatus;
        
        task[i]->thread_info.kernel_sp = (uint64)task[i] + PGSIZE;
        // task[i]->thread_info.user_sp = (uint64)alloc_page();

        task[i]->vma_cnt = 0;

        uint64 pgtbl = alloc_page();
        memcpy((void *)pgtbl, (void *)swapper_pg_dir, PGSIZE);

        load_program(task[i], (uint64 *)pgtbl);
        uint64 va = USER_END - PGSIZE;
        uint64 pa = task[i]->thread_info.user_sp - (uint64)PA2VA_OFFSET;
        do_mmap(task[i], va, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, 0, 0);
        
        pgtbl = (pgtbl - PA2VA_OFFSET);
        task[i]->pgtbl = (uint64 *)pgtbl;
    }
    

    printk("...proc_init done!\n");
}

void switch_to(struct task_struct* next) {
    struct task_struct *tmp_current = current;
    current = next;
    __switch_to(tmp_current, next);
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度
    if (current == idle || !current->counter) {
        schedule();
        return;
    }

    current->counter--;
    if (current->counter)
        return;
    schedule();
    
}

void do_mmap(struct task_struct *task, uint64 addr, uint64 length, uint64 flags,
    uint64 vm_content_offset_in_file, uint64 vm_content_size_in_file) {
    struct vm_area_struct *vma = &(task->vmas[task->vma_cnt]);
    addr = PGROUNDDOWN(addr);
    length = PGROUNDUP(length);
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_flags = flags;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
    task->vma_cnt++;
}

struct vm_area_struct *find_vma(struct task_struct *task, uint64 addr) {
    int cnt = task->vma_cnt;
    for(int i = 0; i < cnt; i++) {
        if(task->vmas[i].vm_start <= addr && task->vmas[i].vm_end > addr)
            return &(task->vmas[i]);
    }
    return NULL;
}

void do_page_fault(uint64 scause, uint64 stval) {
    /*
     1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
     2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
     3. 分配一个页，将这个页映射到对应的用户地址空间
     4. 通过 vma->vm_flags | VM_ANONYM 获得当前的 VMA 是否是匿名空间
     5. 根据 VMA 匿名与否决定将新的页清零或是拷贝 uapp 中的内容
    */
    struct vm_area_struct *vma = find_vma(current, stval);
    if(vma == NULL) {
        printk("not find %lx in vma! pid: %d\n", stval, current->pid);
        while (1);
        return;
    }
    uint64 page = alloc_page();
    int flags = PTE_V | PTE_U;
    if(vma->vm_flags & VM_X_MASK) flags |= PTE_X;
    if(vma->vm_flags & VM_R_MASK) flags |= PTE_R;
    if(vma->vm_flags & VM_W_MASK) flags |= PTE_W;
    if(vma->vm_flags & VM_ANONYM) {
        memset((void *)page, 0x0, PGSIZE);
    } else {
        uint64 src = (uint64)uapp_start + vma->vm_content_offset_in_file;
        uint64 page_offset = (stval - vma->vm_start) / PGSIZE;
        src += page_offset * PGSIZE;
        memcpy((void *)page, (void *)PGROUNDDOWN(src), PGSIZE);
    }
    create_mapping((uint64 *)((uint64)(current->pgtbl) + (uint64)PA2VA_OFFSET), PGROUNDDOWN(stval), page-PA2VA_OFFSET, PGSIZE, flags, 1);
}


void schedule(void) {
    uint64 min_time = 1000000007;
    int min_index = 0, all_zero = 1;
    for (int i = 1; i < NR_TASKS; i++) {
        if (task[i] == NULL)
            break;
        
        if (task[i]->counter < min_time && task[i]->counter != 0) {
            min_time = task[i]->counter;
            min_index = i;
            all_zero = 0;
        }  
    }
    if (!all_zero) {
        printk("switch to [PID = %d COUNTER = %d]\n", min_index, min_time);
        switch_to(task[min_index]);
        return;
    }
    for (int i = 1; i < NR_TASKS; i++) {
        if (task[i] == NULL)
            break;
        task[i]->counter = rand();
        printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
    }
    schedule();
   
}