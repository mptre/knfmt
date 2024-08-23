/*
 * Unwanted trailing newline in warn(3).
 */

int
main(void)
{
	warn("one" "two\n");
	vwarn("one" "two\n", ap);
	warnc(1, "one" "two\n");
	vwarnc(1, "one" "two\n", ap);
	warnx("one" "two\n");
	vwarnx("one" "two\n", ap);
}
