/*
 * Sized int tokens regression.
 */

static void
test_arithmetic(void)
{
	int32_t i32;
	assert(i32_add_overflow(1, 1, &i32) == 0);
}
