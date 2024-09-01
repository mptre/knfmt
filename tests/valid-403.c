/*
 * if/else w/o braces regression.
 */

int
main(void)
{
	if (0)
		switch (c) {
		case 0:
			break;
		}
	else
		return 1;
}
