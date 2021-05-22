/*
 * Hard recover.
 */

struct witness_cpu {
	struct lock_list_entry	*wc_spinlocks;
	struct lock_list_entry	*wc_lle_cache;
	union lock_stack	*wc_stk_cache;
	unsigned int		 wc_lle_count;
	unsigned int		 wc_stk_count;
} __aligned(CACHELINESIZE);
