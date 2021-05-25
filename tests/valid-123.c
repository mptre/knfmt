/*
 * Broken code hidden behind cpp.
 */

#if 0
static void
print_err(int val)
{
	int err;
	printf("Error was %d\n", val);

	while ((err = ERR_get_error()))x {
		const char *msg = (const char*)ERR_reason_error_string(err);
		const char *lib = (const char*)ERR_lib_error_string(err);
		const char *func = (const char*)ERR_func_error_string(err);

		printf("%s in %s %s\n", msg, lib, func);
	}
}
#else
#define print_err(v) ((void)0)
#endif
