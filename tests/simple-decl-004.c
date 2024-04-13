/*
 * Ignore anonymous struct and union.
 */

int
main(void)
{
	union {
		uint64_t u64;
	} u1, u2;

	struct {
		uint64_t u64;
	} s1, s2;
}
