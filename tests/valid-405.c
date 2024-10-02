/*
 * Switch case fallthrough w/o semicolon.
 */

int
main(void)
{
	switch (x) {
	case 0:
		FALLTHROUGH
	default:
		break;
	}
}
