/*
 * Multiple switch cases on the same line.
 */

int
main(void)
{
	switch (x) {
	case '0': case '1': case '2': case '3': case '4': /* argument digit */
	case '5': case '6': case '7': case '8': case '9':
		return 0;
	}
}
