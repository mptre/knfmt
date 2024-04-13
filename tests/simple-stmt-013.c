/*
 * Ignored statement dictates braces.
 */

int
main(void)
{
	if ((lockflags & UVM_LK_EXIT) == 0) {
		vm_map_unlock(map);
	} else {
#ifdef DIAGNOSTIC
		if (timestamp_save != map->timestamp)
			panic("uvm_map_pageable_wire: stale map");
#endif
	}
	return 0;
}
