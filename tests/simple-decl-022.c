/*
 * Regression caused by introduction of simple-stmt-empty-loop pass.
 */

int
main(void)
{
	for (;;) {
		uint8_t j, lo, hi;

		continue;
	}
}
