/*
 * Function pointer in brace initializer.
 */

struct hash_function {
	void	(*init)(void *);
} functions = {
	(void (*)(void *))CKSUM_Init,
};
