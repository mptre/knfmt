/*
 * Redundant curly braces around oneline statement regression.
 */

int
main(void)
{
	switch (0) {
	case 0: {
		if (0) {
			return 0;
		} else {
			doc_alloc(dospace ? DOC_LINE : DOC_SOFTLINE, lhs);
		}
		break;
	}
	}
}
