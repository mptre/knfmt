/*
 * Many lines in field brace initializers.
 */

static const struct {
	int x[10];
	int y[10];
} coordinates[] = {
	.x[0]  = 0, .y[0]  = 0,
	.x[10] = 1, .y[10] = 10,
};
