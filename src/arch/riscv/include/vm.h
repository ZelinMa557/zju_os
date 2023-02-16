#ifndef _VM_H
#define _VM_H
#include "riscv.h"
int create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm, int high);
uint64* get_entry(uint64 *pgtbl, uint64 va, int alloc, int high);
uint64 kvmmap(void);
#endif