/*
 * Unexpected token in enum.
 */

struct a {
	enum {
		|,
	};
};
