/*
 * Unwanted trailing newline in perror(3).
 */

int
main(void)
{
	perror("one\n");
}
