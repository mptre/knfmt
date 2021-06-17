const struct programmer_entry	programmer_table[] = {
#if CONFIG_INTERNAL == 1
#if defined(__i386__) || defined(__x86_64__)
	{},
#endif
#endif
};

struct shutdown_func_data {
	void	 (*func)(void *data);
	void	*data;
} static shutdown_fn[SHUTDOWN_MAXFN];
