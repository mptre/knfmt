/*
 * Expression cpp branch.
 */

void
kmeminit(void)
{
	kmem_map = uvm_km_suballoc(kernel_map, &base, &limit,
	    (vsize_t)nkmempages << PAGE_SHIFT,
#ifdef KVA_GUARDPAGES
	    VM_MAP_INTRSAFE | VM_MAP_GUARDPAGES,
#else
	    VM_MAP_INTRSAFE,
#endif
	    FALSE, &kmem_map_store);
}
