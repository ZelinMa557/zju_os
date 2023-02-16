#include "printk.h"
#include "sbi.h"
#include "defs.h"
#include "proc.h"
#include "mm.h"

extern void test();

int start_kernel() {
    task_init();
    printk("[S MODE] %d hello risc-v\n", 2022);
    schedule();
    test(); // DO NOT DELETE !!!

	return 0;
}
