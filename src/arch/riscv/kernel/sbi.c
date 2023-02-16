#include "types.h"
#include "sbi.h"

// ext->a7, fid->a6
struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
    // unimplemented
	struct sbiret result;
	__asm__ volatile (
		"mv a0, %2\n"
		"mv a1, %3\n"
		"mv a2, %4\n"
		"mv a3, %5\n"
		"mv a4, %6\n"
		"mv a5, %7\n"
		"mv a6, %8\n"
		"mv a7, %9\n"
		"ecall\n"
		"mv %0, a0\n"
		"mv %1, a1\n"
		:"=r"(result.error), "=r"(result.value)
		:"r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5),
		 "r"(fid), "r"(ext)
		: "a0","a1","a2","a3","a4", "a5", "a6", "a7"
	);
	return result;           
}
