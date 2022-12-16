/*
 * Regression caused by token_trim() side effects.
 */

enum {
#if 0
	A,
	B,
#else
	C,
#endif
};
