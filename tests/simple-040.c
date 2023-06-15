/*
 * Missing trailing comma in designated initializers.
 */

struct foo a = {
	.x = 1,
	.y = 1
};

struct foo a = {.x = 1, .y = 1};

enum {
	ONE,
	TWO
};

enum { ONE, TWO };
