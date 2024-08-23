/*
 * Switch case fall through comment.
 */

int
main(void)
{
	switch (x) {
	/* 1 */
	/* 2 */

	/* 3 */
	case 0:
		return 0;
	case 1:
		/* FALLTHROUGH */
	case 2:
		return 2;
	}
}
