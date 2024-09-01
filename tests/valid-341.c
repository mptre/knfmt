/*
 * Declaration and empty loop expression in for loop, trim redundant semicolon
 * regression.
 */

int
main(void)
{
	for (int t = 0;; t++) {
	}
}
