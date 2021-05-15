/*
 * Unterminated cpp branch.
 */

int
main(void)
{
#if KERNBASE == VM_MIN_KERNEL_ADDRESS
	for (kva = VM_MIN_KERNEL_ADDRESS; kva < virtual_avail;
#else
	    kva_end = roundup((vaddr_t)&end, PAGE_SIZE);
	for (kva = KERNBASE; kva < kva_end;
#endif
	    kva += PAGE_SIZE) {
		unsigned long p1i = pl1_i(kva);
		if (pmap_valid_entry(PTE_BASE[p1i]))
			PTE_BASE[p1i] |= pg_g_kern;
	}
#endif
}
