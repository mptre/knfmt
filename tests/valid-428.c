/*
 * Comment w/o spaces preceding argument.
 */

int
main(void)
{
	if (1)
		return foo(/*good=*/0);
	else
		return bar(1, /*bad=*/1);
}
