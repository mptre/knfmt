/*
 * Unwanted trailing newline in err(3).
 */

int
main(void)
{
	err(1, "one" "two\n");
	verr(1, "one" "two\n", ap);
	errc(1, 2, "one" "two\n");
	verrc(1, 2, "one" "two\n", ap);
	errx(1, "one" "two\n");
	verrx(1, "one" "two\n", ap);
}
