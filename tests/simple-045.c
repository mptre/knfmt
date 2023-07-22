/*
 * Semicolon statement not considered empty.
 */

int
main(void)
{
	if (1) {
		/* nothing */;
	}

	if (1)
		/* nothing */;
}
