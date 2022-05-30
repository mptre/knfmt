/*
 * Switch case statement after break.
 */

int
main(void)
{
	if (0) {
		switch (1) {
		case 1:
			break;
bad:
			return ENOMEM;
		default:
			goto bad;
		}
	}
}
