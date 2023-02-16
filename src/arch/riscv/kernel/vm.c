#include "defs.h"
#include "riscv.h"
#include "string.h"
#include "mm.h"
#include "printk.h"
#include "vm.h"

#define VPN2(va) ((va >> 30) & 0x1ff)
#define PTE2PA(pte) ((pte >> 10) << 12)
#define PA2PTE(pa) ((pa >> 12) << 10)

extern char _stext[];
extern char _srodata[];
extern char _sdata[];

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long  early_pgtbl[512] __attribute__((__aligned__(0x1000)));

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void) {
    /* 
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表 
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。 
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    unsigned long *early_pgtbl_ = early_pgtbl;
    if ((unsigned long)early_pgtbl > VM_START) {
        early_pgtbl_ = (unsigned long *)((unsigned long)early_pgtbl - PA2VA_OFFSET);
    }
    
   	memset(early_pgtbl_, 0x0, PGSIZE);
	early_pgtbl_[VPN2(PHY_START)] = (((PHY_START >> 30) << 28) | PTE_V | PTE_R | PTE_W | PTE_X);
	early_pgtbl_[VPN2(VM_START)] = (((PHY_START >> 30) << 28) | PTE_V | PTE_R | PTE_W | PTE_X);
}

uint64* get_entry(uint64 *pgtbl, uint64 va, int alloc, int high) {
    uint64 *pte;
    for (int level = 2; level >= 0; --level) {
        int index = ((va >> (12 + 9 * level)) & 0x1ff);
        pte = &pgtbl[index];
        if ((!(*pte & PTE_V)) && level > 0) {
            if (alloc)
                *pte = (PA2PTE(alloc_page() - PA2VA_OFFSET) | PTE_V);
            else return 0;
        }
        pgtbl = (uint64*)PTE2PA(*pte);
        if (high) {
            pgtbl = (uint64*)((uint64)pgtbl + PA2VA_OFFSET);
        }
    }
    return pte;
}

/* 创建多级页表映射关系 */
int create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm, int high) {
    /*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小
    perm 为映射的读写权限
    high 表示创建映射之前，是否运行在高地址

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    uint64 end = va + sz;
    uint64 *pte;
    for (; va < end; va += PGSIZE, pa += PGSIZE) {
        if ((pte = get_entry(pgtbl, va, 1, high)) != 0)
            *pte = (PA2PTE(pa) | perm);
        else
            return -1;
    }
    return 0;
}

void setup_vm_final(void) {
    memset(swapper_pg_dir, 0x0, PGSIZE);
    uint64 va = VM_START + OPENSBI_SIZE;
    uint64 pa = PHY_START + OPENSBI_SIZE;

    // No OpenSBI mapping required

    // // mapping kernel text X|-|R|V
    create_mapping((uint64*)swapper_pg_dir, va, pa, (uint64)_srodata - (uint64)_stext, PTE_X | PTE_R | PTE_V, 0);

    // // mapping kernel rodata -|-|R|V
    va += (uint64)_srodata - (uint64)_stext;
    pa += (uint64)_srodata - (uint64)_stext;
    create_mapping((uint64*)swapper_pg_dir, va, pa, (uint64)_sdata - (uint64)_srodata, PTE_R | PTE_V, 0);


    // // mapping other memory -|W|R|V
    va += (uint64)_sdata - (uint64)_srodata;
    pa += (uint64)_sdata - (uint64)_srodata;
    create_mapping((uint64*)swapper_pg_dir, va, pa, PHY_END - pa, PTE_W | PTE_R | PTE_V, 0);

    // set satp with swapper_pg_dir
    unsigned long pg_dir_phy = (uint64)swapper_pg_dir - PA2VA_OFFSET;
    asm volatile (
        "li t0, 8\n"
        "slli t0, t0, 60\n"
        "mv t1, %[pg_dir_phy]\n"
        "srli t1, t1, 12\n"
        "add t2, t1, t0\n"
        "csrw satp, t2"
        :
        :[pg_dir_phy]"r"(pg_dir_phy)
        :"memory"
    );

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");

    printk("...vm init done!\n");
    return;
}

uint64 kvmmap(void) {
    uint64 pg_dir = alloc_page();
    memset((void *)pg_dir, 0x0, PGSIZE);
    uint64 va = VM_START + OPENSBI_SIZE;
    uint64 pa = PHY_START + OPENSBI_SIZE;

    // No OpenSBI mapping required

    // // mapping kernel text X|-|R|V
    create_mapping((uint64*)pg_dir, va, pa, (uint64)_srodata - (uint64)_stext, PTE_X | PTE_R | PTE_V, 1);

    // // mapping kernel rodata -|-|R|V
    va += (uint64)_srodata - (uint64)_stext;
    pa += (uint64)_srodata - (uint64)_stext;
    create_mapping((uint64*)pg_dir, va, pa, (uint64)_sdata - (uint64)_srodata, PTE_R | PTE_V, 1);


    // // mapping other memory -|W|R|V
    va += (uint64)_sdata - (uint64)_srodata;
    pa += (uint64)_sdata - (uint64)_srodata;
    create_mapping((uint64*)pg_dir, va, pa, PHY_END - pa, PTE_W | PTE_R | PTE_V, 1);

    return pg_dir;
}