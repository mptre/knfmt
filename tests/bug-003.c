/*
 * Long expression, probably a missing group causing unwanted line breaks.
 */

int
main(void)
{
	if (0) {
		if (1) {
			if (2) {
				assert(0 && "AlwaysBreakAfterReturnType unknown value");
			}
		}
	}
}
