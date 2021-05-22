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

#ifdef DDB
static void	db_witness_add_fullgraph(struct witness *parent);
static void	witness_ddb_compute_levels(void);
static void	witness_ddb_display(int (*)(const char *fmt, ...));
static void	witness_ddb_display_descendants(int (*)(const char *fmt, ...),
    struct witness *, int indent);
static void	witness_ddb_display_list(int (*prnt)(const char *fmt, ...),
    struct witness_list *list);
static void	witness_ddb_level_descendants(struct witness *parent, int l);
static void	witness_ddb_list(struct proc *td);
#endif
