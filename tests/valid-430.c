/*
 * Branches separated with new line followed by a comment.
 */

int
main(void)
{
	if (0) {
		int x = 0;
		return x;
	}

	/* comment */
	else if (1) {
		int y = 1;
		return y;
	}
}
